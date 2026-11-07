#include "engine/Export.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <tuple>

#include "engine/Scheduler.h"
#include "model/Instruments.h"

namespace chimedes::engine
{

namespace
{
    constexpr int ticksPerBeat = 480;

    // Horarios de las pistas que suenan, con su pista.
    std::vector<std::pair<const model::Track*, Schedule>> soundingTracks (const model::Project& project)
    {
        std::vector<std::pair<const model::Track*, Schedule>> out;
        for (const auto& t : project.tracks)
        {
            if (t.muted)
                continue;
            auto result = buildSchedule (t);
            if (result.ok() && result.schedule->cycleLengthBeats > 0.0 && ! result.schedule->events.empty())
                out.emplace_back (&t, *result.schedule);
        }
        return out;
    }

    // Programa General MIDI parecido a cada instrumento integrado.
    int gmProgram (model::InstrumentId id)
    {
        namespace ids = model::instruments;
        switch (id)
        {
            case ids::organ:         return 16;  // órgano de barras
            case ids::electricPiano: return 4;   // piano eléctrico 1
            case ids::bell:          return 14;  // campanas tubulares
            case ids::pluck:         return 24;  // guitarra de nailon
            case ids::pad:           return 89;  // pad cálido
            case ids::bass:          return 33;  // bajo eléctrico (dedos)
            default:                 return -1;  // muestras: sin cambio de programa
        }
    }

    struct Writer
    {
        std::vector<std::uint8_t> bytes;

        void u8 (int v)  { bytes.push_back (static_cast<std::uint8_t> (v & 0xff)); }
        void u16 (int v) { u8 (v >> 8); u8 (v); }
        void u32 (std::uint32_t v) { u8 (static_cast<int> (v >> 24)); u8 (static_cast<int> (v >> 16)); u8 (static_cast<int> (v >> 8)); u8 (static_cast<int> (v)); }
        void text (const std::string& s) { bytes.insert (bytes.end(), s.begin(), s.end()); }

        void varLen (std::uint32_t v)
        {
            std::uint8_t buffer[5];
            int n = 0;
            buffer[n++] = static_cast<std::uint8_t> (v & 0x7f);
            while ((v >>= 7) != 0)
                buffer[n++] = static_cast<std::uint8_t> ((v & 0x7f) | 0x80);
            while (n > 0)
                u8 (buffer[--n]);
        }
    };

    // Un evento de una pista MIDI, en ticks absolutos.
    struct MidiEvent
    {
        std::uint32_t tick;
        int order;                 // a igual tick: primero los note-off, luego los note-on
        std::vector<std::uint8_t> data;
    };

    void writeTrackChunk (Writer& file, std::vector<MidiEvent> events, std::uint32_t endTick)
    {
        std::stable_sort (events.begin(), events.end(), [] (const MidiEvent& a, const MidiEvent& b)
        {
            return std::tie (a.tick, a.order) < std::tie (b.tick, b.order);
        });

        Writer chunk;
        std::uint32_t last = 0;
        for (const auto& e : events)
        {
            chunk.varLen (e.tick - last);
            last = e.tick;
            chunk.bytes.insert (chunk.bytes.end(), e.data.begin(), e.data.end());
        }

        // Fin de pista.
        chunk.varLen (endTick > last ? endTick - last : 0);
        chunk.u8 (0xff); chunk.u8 (0x2f); chunk.u8 (0x00);

        file.text ("MTrk");
        file.u32 (static_cast<std::uint32_t> (chunk.bytes.size()));
        file.bytes.insert (file.bytes.end(), chunk.bytes.begin(), chunk.bytes.end());
    }

    MidiEvent metaText (int type, const std::string& s)
    {
        Writer w;
        w.u8 (0xff); w.u8 (type); w.varLen (static_cast<std::uint32_t> (s.size())); w.text (s);
        return { 0, -2, w.bytes };
    }

    std::uint32_t toTicks (double beats)
    {
        return static_cast<std::uint32_t> (std::llround (std::max (0.0, beats) * ticksPerBeat));
    }
}

int midiChannelFor (model::InstrumentId instrument) noexcept
{
    // Instrumento 1..9 → canales 1..9; 10..15 → 11..16; el 16 comparte el canal 16.
    // (En base cero: 0..8 y 10..15; el 9 es la percusión de General MIDI.)
    const auto id = std::clamp (static_cast<int> (instrument), 1, 16);
    return id <= 9 ? id - 1 : std::min (id, 15);
}

double exportLengthBeats (const model::Project& project, int cycles, double maxBeats)
{
    const auto tracks = soundingTracks (project);
    if (tracks.empty())
        return 0.0;

    double longest = 0.0;
    for (const auto& [t, s] : tracks)
        longest = std::max (longest, s.cycleLengthBeats);

    if (cycles > 0)
        return std::min (longest * cycles, maxBeats);

    // Mínimo común múltiplo en ticks; si se pasa del máximo, tantas vueltas de la más
    // larga como quepan.
    const auto maxTicks = static_cast<std::uint64_t> (maxBeats * ticksPerBeat);
    std::uint64_t lcm = 1;
    for (const auto& [t, s] : tracks)
    {
        const auto ticks = std::max<std::uint64_t> (1, static_cast<std::uint64_t> (std::llround (s.cycleLengthBeats * ticksPerBeat)));
        lcm = lcm / std::gcd (lcm, ticks) * ticks;
        if (lcm > maxTicks)
            return std::max (longest, std::floor (maxBeats / longest) * longest);
    }

    return static_cast<double> (lcm) / ticksPerBeat;
}

std::vector<std::uint8_t> exportMidi (const model::Project& project, double lengthBeats)
{
    const auto tracks = soundingTracks (project);
    const auto endTick = toTicks (lengthBeats);

    Writer file;
    file.text ("MThd");
    file.u32 (6);
    file.u16 (1);                                        // formato 1: pistas simultáneas
    file.u16 (static_cast<int> (tracks.size()) + 1);     // + pista de tempo
    file.u16 (ticksPerBeat);

    // Pista de tempo: compás 4/4 y el BPM del proyecto.
    {
        std::vector<MidiEvent> events;
        events.push_back (metaText (0x03, "Chimedes"));

        const auto microsPerBeat = static_cast<std::uint32_t> (std::llround (60000000.0 / std::max (1.0, project.bpm)));
        events.push_back ({ 0, -1, { 0xff, 0x51, 0x03,
                                     static_cast<std::uint8_t> (microsPerBeat >> 16),
                                     static_cast<std::uint8_t> (microsPerBeat >> 8),
                                     static_cast<std::uint8_t> (microsPerBeat) } });
        events.push_back ({ 0, -1, { 0xff, 0x58, 0x04, 4, 2, 24, 8 } });
        writeTrackChunk (file, std::move (events), endTick);
    }

    for (const auto& [track, schedule] : tracks)
    {
        std::vector<MidiEvent> events;
        events.push_back (metaText (0x03, track->name.empty() ? std::string ("Pista") : track->name));

        // Cambio de programa al principio en cada canal de instrumento integrado.
        std::set<std::pair<int, int>> programs;
        for (const auto& e : schedule.events)
            if (const auto program = gmProgram (e.instrument); program >= 0)
                programs.insert ({ midiChannelFor (e.instrument), program });
        for (const auto& [channel, program] : programs)
            events.push_back ({ 0, -1, { static_cast<std::uint8_t> (0xc0 | channel), static_cast<std::uint8_t> (program) } });

        // Las vueltas del ciclo que caben en la duración; las notas se cortan al final.
        struct Note { std::uint32_t start, end; int channel, pitch, velocity; };
        std::vector<Note> notes;

        for (double offset = 0.0; offset < lengthBeats - 1e-9; offset += schedule.cycleLengthBeats)
            for (const auto& e : schedule.events)
            {
                const auto start = offset + e.startBeat;
                if (start >= lengthBeats - 1e-9)
                    continue;

                const auto end = std::min (start + e.durationBeats, lengthBeats);
                for (const auto pitch : e.pitches)
                    notes.push_back ({ toTicks (start), std::max (toTicks (start) + 1, toTicks (end)),
                                       midiChannelFor (e.instrument), std::clamp (pitch, 0, 127), std::clamp (e.velocity, 1, 127) });
            }

        // La misma nota en el mismo canal no puede solaparse en MIDI: la anterior
        // termina cuando empieza la siguiente.
        std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b)
        {
            return std::tie (a.channel, a.pitch, a.start) < std::tie (b.channel, b.pitch, b.start);
        });
        for (std::size_t i = 0; i + 1 < notes.size(); ++i)
            if (notes[i].channel == notes[i + 1].channel && notes[i].pitch == notes[i + 1].pitch)
                notes[i].end = std::min (notes[i].end, std::max (notes[i].start + 1, notes[i + 1].start));

        // Notas repetidas en el mismo instante (dos nodos iguales a la vez): una sola.
        notes.erase (std::unique (notes.begin(), notes.end(), [] (const Note& a, const Note& b)
        {
            return a.channel == b.channel && a.pitch == b.pitch && a.start == b.start;
        }), notes.end());

        for (const auto& n : notes)
        {
            const auto channel = static_cast<std::uint8_t> (n.channel);
            const auto pitch = static_cast<std::uint8_t> (n.pitch);
            events.push_back ({ n.start, 1, { static_cast<std::uint8_t> (0x90 | channel), pitch, static_cast<std::uint8_t> (n.velocity) } });
            events.push_back ({ n.end, 0, { static_cast<std::uint8_t> (0x80 | channel), pitch, 0 } });
        }

        writeTrackChunk (file, std::move (events), endTick);
    }

    return file.bytes;
}

} // namespace chimedes::engine
