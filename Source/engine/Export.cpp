#include "engine/Export.h"

#include <algorithm>
#include <array>
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

    // Schedules of the tracks that sound, with their track.
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

    // Channels in assignment order: 1..9 and 11..16 (zero-based 0..8 and 10..15).
    constexpr std::array<int, 15> usableChannels { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15 };

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

    // An event in a MIDI track, in absolute ticks.
    struct MidiEvent
    {
        std::uint32_t tick;
        int order;                 // same tick: note-offs first, then note-ons
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

        // End of track.
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

std::map<model::InstrumentId, int> midiChannels (const model::Project& project)
{
    std::map<model::InstrumentId, int> channels;
    for (const auto& [track, schedule] : soundingTracks (project))
        for (const auto& e : schedule.events)
            if (channels.count (e.instrument) == 0)
            {
                const auto next = channels.size() % usableChannels.size();
                channels.emplace (e.instrument, usableChannels[next]);
            }
    return channels;
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

    // Least common multiple in ticks; if it goes over the maximum, as many loops of the
    // longest track as fit.
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
    const auto channels = midiChannels (project);
    const auto channelOf = [&channels] (model::InstrumentId id) { return channels.at (id); };

    Writer file;
    file.text ("MThd");
    file.u32 (6);
    file.u16 (1);                                        // format 1: simultaneous tracks
    file.u16 (static_cast<int> (tracks.size()) + 1);     // + tempo track
    file.u16 (ticksPerBeat);

    // Tempo track: 4/4 time and the project's BPM.
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
        events.push_back (metaText (0x03, track->name.empty() ? std::string ("Track") : track->name));

        // Program change at the start on every built-in instrument channel.
        std::set<std::pair<int, int>> programs;
        for (const auto& e : schedule.events)
            if (const auto program = model::instruments::gmProgram (e.instrument); program >= 0)
                programs.insert ({ channelOf (e.instrument), program });
        for (const auto& [channel, program] : programs)
            events.push_back ({ 0, -1, { static_cast<std::uint8_t> (0xc0 | channel), static_cast<std::uint8_t> (program) } });

        // The cycle loops that fit in the length; notes are cut at the end.
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
                                       channelOf (e.instrument), std::clamp (pitch, 0, 127), std::clamp (e.velocity, 1, 127) });
            }

        // The same note on the same channel cannot overlap in MIDI: the previous one
        // ends when the next one starts.
        std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b)
        {
            return std::tie (a.channel, a.pitch, a.start) < std::tie (b.channel, b.pitch, b.start);
        });
        for (std::size_t i = 0; i + 1 < notes.size(); ++i)
            if (notes[i].channel == notes[i + 1].channel && notes[i].pitch == notes[i + 1].pitch)
                notes[i].end = std::min (notes[i].end, std::max (notes[i].start + 1, notes[i + 1].start));

        // Repeated notes at the same instant (two identical nodes at once): keep one.
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
