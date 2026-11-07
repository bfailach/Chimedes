#include "TestFramework.h"

#include <array>
#include <cstdint>
#include <string>
#include <map>
#include <set>

#include "engine/Export.h"
#include "engine/Scheduler.h"
#include "model/DemoProject.h"
#include "model/Instruments.h"
#include "model/Randomizer.h"

using namespace chimedes;

namespace
{
    // Minimal reader of the MIDI we write, to check it.
    struct ParsedMidi
    {
        int format = -1, trackCount = -1, division = -1;
        std::uint32_t microsPerBeat = 0;
        std::vector<std::string> names;
        int noteOns = 0, noteOffs = 0;
        std::set<int> channels;
        std::map<int, int> programs;   // channel → program
        bool balanced = true;          // every note-on with its note-off
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
                pos++;   // velocity
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
        t.name = "Melody";
        p.tracks = { t };
        return p;
    }
}

TEST ("midi: header, tempo, name and one note per scheduled note")
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
    CHECK (m.names.size() == 2 && m.names[1] == "Melody");

    int expected = 0;
    for (const auto& e : schedule.events)
        expected += static_cast<int> (e.pitches.size());
    CHECK (m.noteOns == 2 * expected);
    CHECK (m.noteOns == m.noteOffs);
    CHECK (m.balanced);
    CHECK (m.lastTick == static_cast<std::uint32_t> (length * 480));
}

TEST ("midi: muted tracks left out, never channel 10, General MIDI program per instrument")
{
    // 16 tracks: built-ins (old and new ids) and samples.
    const std::array<model::InstrumentId, 16> ids { 1, 6, 3, 7, 17, 25, 9, 32, 2, 4, 5, 20, 26, 28, 8, 30 };

    model::Project p;
    for (std::uint32_t i = 1; i <= 16; ++i)
    {
        model::Track t;
        t.id = i;
        t.instrument = ids[i - 1];
        t.muted = i == 3;
        model::randomizeGraph (t, i);
        p.tracks.push_back (t);
    }

    const auto channels = engine::midiChannels (p);
    CHECK (channels.size() == 15);                // the muted bell is left out
    CHECK (channels.count (model::instruments::bell) == 0);
    CHECK (channels.at (1) == 0 && channels.at (6) == 1 && channels.at (7) == 2);

    std::set<int> distinct;
    for (const auto& [id, channel] : channels)
        distinct.insert (channel);
    CHECK (distinct.size() == 15);                // 15 instruments, 15 channels
    CHECK (distinct.count (9) == 0);

    const auto m = parse (engine::exportMidi (p, engine::exportLengthBeats (p, 1, 1000.0)));
    CHECK (m.trackCount == 16);                   // 15 playing + tempo
    CHECK (m.channels.count (9) == 0);            // percussion
    CHECK (m.programs.at (channels.at (1)) == 16);    // organ
    CHECK (m.programs.at (channels.at (6)) == 33);    // bass
    CHECK (m.programs.at (channels.at (17)) == 0);    // piano
    CHECK (m.programs.at (channels.at (25)) == 48);   // strings
    CHECK (m.programs.count (channels.at (7)) == 0);  // sample: no program
    CHECK (m.balanced);
}

TEST ("midi: more than 15 instruments share channels again, still never channel 10")
{
    model::Project p;
    std::uint32_t trackId = 1;
    for (const auto id : model::instruments::builtInIds())
    {
        model::Track t;
        t.id = trackId++;
        t.instrument = id;
        model::randomizeGraph (t, t.id);
        p.tracks.push_back (t);
    }

    const auto channels = engine::midiChannels (p);
    CHECK (channels.size() == model::instruments::builtIns.size());
    for (const auto& [id, channel] : channels)
        CHECK (channel >= 0 && channel <= 15 && channel != 9);

    const auto m = parse (engine::exportMidi (p, engine::exportLengthBeats (p, 1, 1000.0)));
    CHECK (m.balanced);
    CHECK (m.channels.count (9) == 0);
}

TEST ("instruments: the catalogue is consistent")
{
    std::set<model::InstrumentId> seen;
    for (const auto& i : model::instruments::builtIns)
    {
        CHECK (seen.insert (i.id).second);                      // unique ids
        CHECK (! model::instruments::isSample (i.id));          // never a sample id
        CHECK (i.id >= 1 && i.id <= model::instruments::maxId);
        CHECK (i.gmProgram >= 0 && i.gmProgram <= 127);
        CHECK (std::string (i.name).size() > 0);
        CHECK (model::instruments::isValid (i.id));
    }
    CHECK (seen.size() == 22);

    // The original six keep their ids (project files store them).
    CHECK (std::string (model::instruments::name (1)) == "Organ");
    CHECK (std::string (model::instruments::name (6)) == "Bass");
    CHECK (! model::instruments::isValid (0));
    CHECK (! model::instruments::isValid (33));
    CHECK (model::instruments::isValid (16));                   // last sample
}

TEST ("midi: overlapping repeated notes leave no hanging notes")
{
    auto t = model::makeDemoTrack();
    for (auto& n : t.nodes)
    {
        n.pitches = { 60 };
        n.durationBeats = 8.0;      // they all overlap each other
    }

    const auto m = parse (engine::exportMidi (oneTrack (t), 16.0));
    CHECK (m.balanced);
    CHECK (m.noteOns == m.noteOffs);
}

TEST ("length: full loop = least common multiple of the cycles")
{
    // Two tracks of 3 and 4 beats line up every 12.
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
    p.tracks = { track (1, 200.0), track (2, 300.0) };   // cycles of 3 and 4 beats
    CHECK (engine::exportLengthBeats (p, 0, 1000.0) == 12.0);
    CHECK (engine::exportLengthBeats (p, 3, 1000.0) == 12.0);   // 3 loops of the 4-beat one
    CHECK (engine::exportLengthBeats (p, 0, 10.0) == 8.0);      // cap: whole loops of the longest

    p.tracks[0].muted = true;
    CHECK (engine::exportLengthBeats (p, 0, 1000.0) == 4.0);
    p.tracks[1].muted = true;
    CHECK (engine::exportLengthBeats (p, 0, 1000.0) == 0.0);
}
