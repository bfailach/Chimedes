#include "TestFramework.h"

#include <algorithm>
#include <cmath>

#include "engine/Scheduler.h"
#include "model/Randomizer.h"

using namespace chimedes;

TEST ("random: the graph can always be played, in every mode")
{
    int failures = 0;

    for (std::uint32_t seed = 1; seed <= 300; ++seed)
    {
        for (const bool directed : { true, false })
        {
            model::Track t;
            t.directed = directed;
            model::randomizeGraph (t, seed);

            for (const auto mode : { model::PlaybackMode::Timeline, model::PlaybackMode::Algorithm })
            {
                for (const auto method : { model::TreeMethod::BFS, model::TreeMethod::DFS, model::TreeMethod::Dijkstra })
                {
                    t.mode = mode;
                    t.treeMethod = method;
                    failures += engine::buildSchedule (t).ok() ? 0 : 1;
                }
            }
        }
    }

    CHECK (failures == 0);
}

TEST ("random: node count, start on the left and end on the right")
{
    for (std::uint32_t seed = 1; seed <= 200; ++seed)
    {
        model::Track t;
        model::randomizeGraph (t, seed);

        CHECK (t.nodes.size() >= 5 && t.nodes.size() <= 14);

        const auto [minIt, maxIt] = std::minmax_element (t.nodes.begin(), t.nodes.end(),
            [] (const model::Node& a, const model::Node& b) { return a.x < b.x; });

        CHECK (t.findNode (t.startNode)->x == minIt->x);
        CHECK (t.findNode (t.endNode)->x == maxIt->x);
        CHECK (minIt->x >= 0.0);

        // The overall length is one of the expected ones: from the start until the
        // last note ends, in beats.
        const auto span = (maxIt->x - minIt->x) / 100.0 + t.findNode (t.endNode)->durationBeats;
        CHECK (span == 4.0 || span == 6.0 || span == 8.0 || span == 12.0 || span == 16.0);

        for (const auto& n : t.nodes)
        {
            CHECK (! n.pitches.empty());
            for (const auto p : n.pitches)
                CHECK (p >= 0 && p <= 127);
        }
    }
}

TEST ("random: the same seed gives the same graph and another seed, another")
{
    model::Track a, b, c;
    model::randomizeGraph (a, 1234);
    model::randomizeGraph (b, 1234);
    model::randomizeGraph (c, 1235);

    CHECK (a == b);
    CHECK (! (a == c));
}

TEST ("random: keeps the track's options")
{
    model::Track t;
    t.mode = model::PlaybackMode::Algorithm;
    t.instrument = 3;
    t.quantizeBeats = 0.5;
    t.directed = false;

    model::randomizeGraph (t, 7);

    CHECK (t.mode == model::PlaybackMode::Algorithm);
    CHECK (t.instrument == 3);
    CHECK (t.quantizeBeats == 0.5);
    CHECK (! t.directed);
}

TEST ("random: all four shapes and chords show up")
{
    // Indirect signs of variety over 200 seeds: chains (nobody with more than one
    // child), graphs with branches, graphs with more edges than nodes - 1 (web) and chords.
    int chains = 0, branched = 0, webs = 0, withChords = 0;

    for (std::uint32_t seed = 1; seed <= 200; ++seed)
    {
        model::Track t;
        model::randomizeGraph (t, seed);

        std::vector<int> children (t.nodes.size() + 1, 0);
        for (const auto& e : t.edges)
            ++children[e.from];

        const auto maxChildren = *std::max_element (children.begin(), children.end());
        chains += maxChildren == 1 ? 1 : 0;
        branched += maxChildren > 1 ? 1 : 0;
        webs += t.edges.size() > t.nodes.size() - 1 ? 1 : 0;
        withChords += std::any_of (t.nodes.begin(), t.nodes.end(), [] (const model::Node& n) { return n.pitches.size() > 1; }) ? 1 : 0;
    }

    CHECK (chains > 10);
    CHECK (branched > 10);
    CHECK (webs > 10);
    CHECK (withChords > 10);
}

TEST ("random: notes land on half beats and the cycle lasts whole beats")
{
    // If notes fell between beats, the music would not follow the BPM.
    for (std::uint32_t seed = 1; seed <= 300; ++seed)
    {
        model::Track t;
        model::randomizeGraph (t, seed);

        const auto result = engine::buildSchedule (t);
        CHECK (result.ok());
        const auto& s = *result.schedule;

        for (const auto& e : s.events)
            CHECK (std::abs (e.startBeat * 2.0 - std::round (e.startBeat * 2.0)) < 1e-9);

        CHECK (std::abs (s.cycleLengthBeats - std::round (s.cycleLengthBeats)) < 1e-9);

        // No note shares a position with another.
        for (std::size_t i = 1; i < s.events.size(); ++i)
            CHECK (s.events[i].startBeat > s.events[i - 1].startBeat);
    }
}

// ---------------------------------------------------------------------------
// Randomizing one thing only
// ---------------------------------------------------------------------------

namespace
{
    // Same shape: same nodes (id and position) and same edges.
    bool sameShape (const model::Track& a, const model::Track& b)
    {
        if (a.nodes.size() != b.nodes.size() || ! (a.edges == b.edges) || a.startNode != b.startNode || a.endNode != b.endNode)
            return false;
        for (std::size_t i = 0; i < a.nodes.size(); ++i)
            if (a.nodes[i].id != b.nodes[i].id || a.nodes[i].x != b.nodes[i].x || a.nodes[i].y != b.nodes[i].y)
                return false;
        return true;
    }

    model::Track randomTrack (std::uint32_t seed, bool directed = true)
    {
        model::Track t;
        t.id = 1;
        t.directed = directed;
        model::randomizeGraph (t, seed);
        return t;
    }
}

TEST ("random notes: changes the notes, keeps the shape and ends on the tonic")
{
    for (std::uint32_t seed = 1; seed <= 100; ++seed)
    {
        const auto before = randomTrack (seed);
        auto t = before;
        model::randomizeNotes (t, seed + 1000);

        CHECK (sameShape (before, t));
        for (const auto& n : t.nodes)
        {
            CHECK (! n.pitches.empty());
            for (const auto p : n.pitches)
                CHECK (p >= 24 && p <= 108);
        }
        CHECK (t.findNode (t.endNode)->pitches.size() == 1);
        CHECK (engine::buildSchedule (t).ok());
    }

    // Same seed, same result.
    auto a = randomTrack (7), b = randomTrack (7);
    model::randomizeNotes (a, 99);
    model::randomizeNotes (b, 99);
    CHECK (a == b);
}

TEST ("random rhythm: keeps the shape and the end node's duration")
{
    for (std::uint32_t seed = 1; seed <= 100; ++seed)
    {
        const auto before = randomTrack (seed);
        auto t = before;
        model::randomizeRhythm (t, seed * 3);

        CHECK (sameShape (before, t));
        CHECK (t.findNode (t.endNode)->durationBeats == before.findNode (before.endNode)->durationBeats);
        for (const auto& n : t.nodes)
        {
            CHECK (n.durationBeats >= 0.25 && n.durationBeats <= 2.0);
            CHECK (n.velocity >= 30 && n.velocity <= 127);
        }

        // In timeline mode the cycle keeps its length.
        CHECK (engine::buildSchedule (t).schedule->cycleLengthBeats == engine::buildSchedule (before).schedule->cycleLengthBeats);
    }
}

TEST ("random instruments: only uses the available ones")
{
    const std::vector<model::InstrumentId> available { 2, 5, 7 };
    for (std::uint32_t seed = 1; seed <= 100; ++seed)
    {
        auto t = randomTrack (seed);
        model::randomizeInstruments (t, seed, available);

        CHECK (std::find (available.begin(), available.end(), t.instrument) != available.end());
        for (const auto& n : t.nodes)
            CHECK (n.instrument == 0 || (std::find (available.begin(), available.end(), n.instrument) != available.end()
                                         && n.instrument != t.instrument));
    }

    auto t = randomTrack (3);
    const auto before = t;
    model::randomizeInstruments (t, 3, {});
    CHECK (t == before);
}

TEST ("random algorithm: Prim only on undirected graphs and always playable")
{
    for (std::uint32_t seed = 1; seed <= 200; ++seed)
    {
        for (const bool directed : { true, false })
        {
            auto t = randomTrack (seed, directed);
            t.mode = model::PlaybackMode::Algorithm;
            model::randomizeAlgorithm (t, seed);

            CHECK (t.mode == model::PlaybackMode::Algorithm);
            if (directed)
                CHECK (t.treeMethod != model::TreeMethod::Prim);
            CHECK (engine::buildSchedule (t).ok());
        }
    }
}

TEST ("random everything: always playable and keeps the track's options")
{
    const std::vector<model::InstrumentId> available { 1, 2, 3, 4, 5, 6 };
    for (std::uint32_t seed = 1; seed <= 200; ++seed)
    {
        model::Track t;
        t.id = 4;
        t.name = "Bass";
        t.directed = seed % 2 == 0;
        t.mode = seed % 3 == 0 ? model::PlaybackMode::Algorithm : model::PlaybackMode::Timeline;
        const auto before = t;

        model::randomizeEverything (t, seed, available);

        CHECK (engine::buildSchedule (t).ok());
        CHECK (t.id == before.id && t.name == before.name && t.directed == before.directed && t.mode == before.mode);
    }
}
