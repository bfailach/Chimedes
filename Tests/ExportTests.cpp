#include "TestFramework.h"

#include <cstdint>
#include <map>
#include <set>

#include "engine/Export.h"
#include "engine/Scheduler.h"
#include "model/DemoProject.h"
#include "model/Randomizer.h"

using namespace chimedes;

namespace
{
    // Lector mínimo del MIDI que escribimos, para comprobarlo.
    struct ParsedMidi
    {
        int format = -1, trackCount = -1, division = -1;
        std::uint32_t microsPerBeat = 0;
        std::vector<std::string> names;
        int noteOns = 0, noteOffs = 0;
        std::set<int> channels;
        std::map<int, int> programs;   // canal → programa
        bool balanced = true;          // cada note-on con su note-off
        std::uint32_t lastTick = 0;
    };

    ParsedMidi parse (const std::vector<std::uint8_t>& b)
    {
        ParsedMidi m;
        std::size_t pos = 0;
        auto u16 = [&] { const auto v = (b[pos] << 8) | b[pos + 1]; pos += 2; return v; };
        auto u32 = [&] { std::uint32_t v = 0; for (int i = 0; i < 4; ++i) v = (v << 8) | b[pos++]; return v; };
        auto var = [&] { std::uint32_t v = 0; std::uint8_t c; do { c = b[pos++]; v = (v << 7) | (c & 0x7f); } while (c & 0x80); return v; };

        if (b.size() < 14 || std::string (b.begin(), b.begin() + 4) != "MThd")
            return m;
        pos = 8;
        m.format = u16(); m.trackCount = u16(); m.division = u16();

        for (int t = 0; t < m.trackCount && pos + 8 <= b.size(); ++t)
        {
            if (std::string (b.begin() + static_cast<std::ptrdiff_t> (pos), b.begin() + static_cast<std::ptrdiff_t> (pos) + 4) != "MTrk")
                return m;
            pos += 4;
            const auto length = u32();
            const auto end = pos + length;
            std::uint32_t tick = 0;
            std::map<std::pair<int, int>, int> open;

            while (pos < end)
            {
                tick += var();
                const auto status = b[pos++];
                if (status == 0xff)
                {
                    const auto type = b[pos++];
                    const auto len = var();
                    if (type == 0x03) m.names.emplace_back (b.begin() + static_cast<std::ptrdiff_t> (pos), b.begin() + static_cast<std::ptrdiff_t> (pos + len));
                    if (type == 0x51) m.microsPerBeat = (b[pos] << 16) | (b[pos + 1] << 8) | b[pos + 2];
                    pos += len;
                    continue;
                }

                const auto kind = status & 0xf0, channel = status & 0x0f;
                if (kind == 0xc0) { m.programs[channel] = b[pos++]; continue; }

                const auto pitch = b[pos++];
                pos++;   // velocidad
                m.channels.insert (channel);
                if (kind == 0x90) { ++m.noteOns; ++open[{ channel, pitch }]; }
                if (kind == 0x80) { ++m.noteOffs; if (--open[{ channel, pitch }] < 0) m.balanced = false; }
            }

            for (const auto& [key, count] : open)
                if (count != 0)
                    m.balanced = false;
            m.lastTick = std::max (m.lastTick, tick);
        }
        return m;
    }

    model::Project oneTrack (model::Track t, double bpm = 120.0)
    {
        model::Project p;
        p.bpm = bpm;
        t.id = 1;
        t.name = "Melodía";
        p.tracks = { t };
        return p;
    }
}

TEST ("midi: cabecera, tempo, nombre y una nota por cada nota del horario")
{
    const auto project = oneTrack (model::makeDemoTrack(), 96.0);
    const auto schedule = *engine::buildSchedule (project.tracks[0]).schedule;
    const auto length = engine::exportLengthBeats (project, 2, 1000.0);
    CHECK (length == 2 * schedule.cycleLengthBeats);

    const auto m = parse (engine::exportMidi (project, length));
    CHECK (m.format == 1);
    CHECK (m.trackCount == 2);
    CHECK (m.division == 480);
    CHECK (m.microsPerBeat == 625000);   // 96 BPM
    CHECK (m.names.size() == 2 && m.names[1] == "Melodía");

    int expected = 0;
    for (const auto& e : schedule.events)
        expected += static_cast<int> (e.pitches.size());
    CHECK (m.noteOns == 2 * expected);
    CHECK (m.noteOns == m.noteOffs);
    CHECK (m.balanced);
    CHECK (m.lastTick == static_cast<std::uint32_t> (length * 480));
}

TEST ("midi: pistas silenciadas fuera, canal 10 nunca, programa General MIDI por instrumento")
{
    model::Project p;
    for (std::uint32_t i = 1; i <= 16; ++i)
    {
        model::Track t;
        t.id = i;
        t.instrument = i;           // los 16 instrumentos, también los de muestra
        t.muted = i == 3;
        model::randomizeGraph (t, i);
        p.tracks.push_back (t);
    }

    const auto m = parse (engine::exportMidi (p, engine::exportLengthBeats (p, 1, 1000.0)));
    CHECK (m.trackCount == 16);                   // 15 que suenan + tempo
    CHECK (m.channels.count (9) == 0);            // percusión
    CHECK (m.programs.at (engine::midiChannelFor (1)) == 16);   // órgano
    CHECK (m.programs.at (engine::midiChannelFor (6)) == 33);   // bajo
    CHECK (m.programs.count (engine::midiChannelFor (7)) == 0); // muestra: sin programa
    CHECK (m.balanced);
}

TEST ("midi: notas repetidas que se solapan no dejan notas colgadas")
{
    auto t = model::makeDemoTrack();
    for (auto& n : t.nodes)
    {
        n.pitches = { 60 };
        n.durationBeats = 8.0;      // todas se pisan entre sí
    }

    const auto m = parse (engine::exportMidi (oneTrack (t), 16.0));
    CHECK (m.balanced);
    CHECK (m.noteOns == m.noteOffs);
}

TEST ("duración: vuelta completa = mínimo común múltiplo de los ciclos")
{
    // Dos pistas de 3 y 4 pulsos coinciden cada 12.
    const auto track = [] (std::uint32_t id, double endX)
    {
        model::Track t;
        t.id = id;
        model::Node a; a.id = 1; a.x = 0;    a.durationBeats = 1.0;
        model::Node b; b.id = 2; b.x = endX; b.durationBeats = 1.0;
        t.nodes = { a, b };
        t.edges = { { 1, 1, 2 } };
        t.startNode = 1;
        t.endNode = 2;
        return t;
    };

    model::Project p;
    p.tracks = { track (1, 200.0), track (2, 300.0) };   // ciclos de 3 y 4 pulsos
    CHECK (engine::exportLengthBeats (p, 0, 1000.0) == 12.0);
    CHECK (engine::exportLengthBeats (p, 3, 1000.0) == 12.0);   // 3 vueltas de la de 4
    CHECK (engine::exportLengthBeats (p, 0, 10.0) == 8.0);      // tope: vueltas enteras de la más larga

    p.tracks[0].muted = true;
    CHECK (engine::exportLengthBeats (p, 0, 1000.0) == 4.0);
    p.tracks[1].muted = true;
    CHECK (engine::exportLengthBeats (p, 0, 1000.0) == 0.0);
}
