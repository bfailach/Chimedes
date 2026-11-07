#include "TestFramework.h"
#include "TestGraphs.h"

#include "engine/Scheduler.h"

using namespace chimedes;
using test::node;
using test::edge;
using Error = engine::ScheduleResult::Error;

namespace
{
    // Finds a node's event; nullptr if that node does not sound.
    const engine::NoteEvent* eventOf (const engine::Schedule& s, model::NodeId id)
    {
        for (const auto& e : s.events)
            if (e.node == id)
                return &e;
        return nullptr;
    }

    // A tree with round lengths (100 px = 1 beat):
    //
    //            A(0,0)
    //       100 ╱      ╲ 200
    //      B(100,0)    C(0,200)
    //   200 │            │ 300
    //      D(300,0)    E(0,500)
    //
    // BFS visits A, B, C, D, E.   DFS visits A, B, D, C, E.
    model::Track branching (model::NodeId end = 5)
    {
        model::Track t;
        t.directed = true;
        t.mode = model::PlaybackMode::Algorithm;
        t.treeMethod = model::TreeMethod::BFS;
        t.nodes = { node (1, 0, 0), node (2, 100, 0), node (3, 0, 200),
                    node (4, 300, 0), node (5, 0, 500) };
        t.edges = { edge (1, 2), edge (1, 3), edge (2, 4), edge (3, 5) };
        t.startNode = 1;
        t.endNode = end;
        return t;
    }
}

// ---------------------------------------------------------------------------
// Timeline mode
// ---------------------------------------------------------------------------

TEST ("timeline: each node sounds at x / 100 from the start")
{
    model::Track t;
    t.directed = false;
    t.nodes = { node (1, 100, 0), node (2, 250, 80), node (3, 400, 0, 2.0),
                node (4, 50, 0), node (5, 500, 0) };
    t.edges = { edge (4, 1), edge (1, 2), edge (2, 3), edge (3, 5) };
    t.startNode = 1;
    t.endNode = 3;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    const auto& s = *result.schedule;

    REQUIRE (s.events.size() == 3);
    CHECK_NEAR (eventOf (s, 1)->startBeat, 0.0);
    CHECK_NEAR (eventOf (s, 2)->startBeat, 1.5);
    CHECK_NEAR (eventOf (s, 3)->startBeat, 3.0);

    // Nodes outside the start–end stretch do not sound.
    CHECK (eventOf (s, 4) == nullptr);
    CHECK (eventOf (s, 5) == nullptr);

    // The cycle ends when the end node finishes sounding (3 + 2 beats).
    CHECK_NEAR (s.cycleLengthBeats, 5.0);
}

TEST ("timeline: the end left of the start is an error")
{
    model::Track t;
    t.directed = false;
    t.nodes = { node (1, 300, 0), node (2, 100, 0) };
    t.edges = { edge (1, 2) };
    t.startNode = 1;
    t.endNode = 2;

    const auto result = engine::buildSchedule (t);
    CHECK (result.error == Error::EndBeforeStart);
    CHECK (! result.schedule.has_value());
}

TEST ("schedule: an invalid track produces no schedule")
{
    auto t = branching();
    t.nodes.push_back (node (9, 900, 900));  // isolated

    const auto result = engine::buildSchedule (t);
    CHECK (result.error == Error::Invalid);
    CHECK (result.validation.unreachable.size() == 1);
    CHECK (! result.schedule.has_value());
}

// ---------------------------------------------------------------------------
// Algorithm mode
// ---------------------------------------------------------------------------

TEST ("sequential algorithm with BFS: waits for each node's parent edge")
{
    auto t = branching();
    t.traversal = model::Traversal::BFS;
    t.siblings = model::SiblingMode::Sequential;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    const auto& s = *result.schedule;

    REQUIRE (s.events.size() == 5);
    CHECK_NEAR (eventOf (s, 1)->startBeat, 0.0);  // A
    CHECK_NEAR (eventOf (s, 2)->startBeat, 1.0);  // B: + AB 100
    CHECK_NEAR (eventOf (s, 3)->startBeat, 3.0);  // C: + AC 200
    CHECK_NEAR (eventOf (s, 4)->startBeat, 5.0);  // D: + BD 200
    CHECK_NEAR (eventOf (s, 5)->startBeat, 8.0);  // E: + CE 300
    CHECK_NEAR (s.cycleLengthBeats, 9.0);
}

TEST ("sequential algorithm with DFS: the same graph sounds in another order")
{
    auto t = branching();
    t.traversal = model::Traversal::DFS;
    t.siblings = model::SiblingMode::Sequential;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    const auto& s = *result.schedule;

    CHECK_NEAR (eventOf (s, 1)->startBeat, 0.0);  // A
    CHECK_NEAR (eventOf (s, 2)->startBeat, 1.0);  // B
    CHECK_NEAR (eventOf (s, 4)->startBeat, 3.0);  // D before C
    CHECK_NEAR (eventOf (s, 3)->startBeat, 5.0);  // C
    CHECK_NEAR (eventOf (s, 5)->startBeat, 8.0);  // E
}

TEST ("simultaneous algorithm: siblings sound together and wait for the shortest edge")
{
    auto t = branching();
    t.traversal = model::Traversal::BFS;
    t.siblings = model::SiblingMode::Simultaneous;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    const auto& s = *result.schedule;

    REQUIRE (s.events.size() == 5);
    CHECK_NEAR (eventOf (s, 1)->startBeat, 0.0);  // A
    CHECK_NEAR (eventOf (s, 2)->startBeat, 1.0);  // B and C together: min(100, 200)
    CHECK_NEAR (eventOf (s, 3)->startBeat, 1.0);
    CHECK_NEAR (eventOf (s, 4)->startBeat, 3.0);  // D: + 200
    CHECK_NEAR (eventOf (s, 5)->startBeat, 6.0);  // E: + 300
    CHECK_NEAR (s.cycleLengthBeats, 7.0);
}

TEST ("algorithm: the cycle ends at the end node even with nodes left")
{
    auto t = branching (3);  // end at C
    t.traversal = model::Traversal::BFS;
    t.siblings = model::SiblingMode::Sequential;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    const auto& s = *result.schedule;

    CHECK (s.events.size() == 3);         // A, B, C
    CHECK (eventOf (s, 4) == nullptr);    // D and E never sound
    CHECK (eventOf (s, 5) == nullptr);
    CHECK_NEAR (s.cycleLengthBeats, 4.0); // C starts at 3 and lasts 1
}

TEST ("algorithm: start and end on the same node")
{
    auto t = branching (1);
    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());

    CHECK (result.schedule->events.size() == 1);
    CHECK_NEAR (result.schedule->cycleLengthBeats, 1.0);
}

TEST ("algorithm: Prim on a directed graph produces no schedule")
{
    auto t = branching();
    t.treeMethod = model::TreeMethod::Prim;

    CHECK (engine::buildSchedule (t).error == Error::TreeUnavailable);
}

TEST ("schedule: events come out sorted by time")
{
    auto t = branching();
    t.traversal = model::Traversal::DFS;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());

    const auto& events = result.schedule->events;
    for (std::size_t i = 1; i < events.size(); ++i)
        CHECK (events[i - 1].startBeat <= events[i].startBeat);
}

// ---------------------------------------------------------------------------
// Optional beat snapping
// ---------------------------------------------------------------------------

#include "model/DemoProject.h"

TEST ("beat snapping: each wait is rounded to the subdivision")
{
    auto t = model::makeDemoTrack();
    t.mode = model::PlaybackMode::Algorithm;
    t.treeMethod = model::TreeMethod::BFS;
    t.traversal = model::Traversal::BFS;
    t.siblings = model::SiblingMode::Sequential;
    t.quantizeBeats = 0.5;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    const auto& s = *result.schedule;

    // Free waits:     1.41 · 1.92 · 1.55 · 1.80 · 2.09 · 1.72 · 1.80
    // Snapped to 1/2: 1.5  · 2.0  · 1.5  · 2.0  · 2.0  · 1.5  · 2.0
    CHECK_NEAR (eventOf (s, 1)->startBeat, 0.0);
    CHECK_NEAR (eventOf (s, 2)->startBeat, 1.5);
    CHECK_NEAR (eventOf (s, 3)->startBeat, 3.5);
    CHECK_NEAR (eventOf (s, 4)->startBeat, 5.0);
    CHECK_NEAR (eventOf (s, 5)->startBeat, 7.0);
    CHECK_NEAR (eventOf (s, 7)->startBeat, 9.0);
    CHECK_NEAR (eventOf (s, 6)->startBeat, 10.5);
    CHECK_NEAR (eventOf (s, 8)->startBeat, 12.5);
    CHECK_NEAR (s.cycleLengthBeats, 14.5);
}

TEST ("beat snapping: a very short edge waits at least one subdivision")
{
    model::Track t;
    t.mode = model::PlaybackMode::Algorithm;
    t.nodes = { node (1, 0, 0), node (2, 10, 0) };  // 10 px = 0.1 beats
    t.edges = { edge (1, 2) };
    t.startNode = 1;
    t.endNode = 2;
    t.quantizeBeats = 0.25;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    CHECK_NEAR (eventOf (*result.schedule, 2)->startBeat, 0.25);
}

TEST ("beat snapping: without snapping, waits are free")
{
    auto t = model::makeDemoTrack();
    t.mode = model::PlaybackMode::Algorithm;
    t.quantizeBeats = 0.0;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    CHECK (std::abs (eventOf (*result.schedule, 2)->startBeat - 1.41421356) < 1e-6);
}

// ---------------------------------------------------------------------------
// Instruments
// ---------------------------------------------------------------------------

#include "model/Instruments.h"

TEST ("instruments: each node uses its own or, if it has none, the track's")
{
    auto t = branching();
    t.instrument = model::instruments::pad;

    for (auto& n : t.nodes)
        if (n.id == 3)
            n.instrument = model::instruments::bell;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());

    CHECK (eventOf (*result.schedule, 1)->instrument == model::instruments::pad);
    CHECK (eventOf (*result.schedule, 3)->instrument == model::instruments::bell);
}

TEST ("instruments: an invalid id falls back to the track's or to the organ")
{
    auto t = branching();
    t.instrument = 99;
    t.nodes[0].instrument = 42;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    CHECK (eventOf (*result.schedule, 1)->instrument == model::instruments::organ);
}

// ---------------------------------------------------------------------------
// Source of each note (the UI uses it to animate the edge travelled)
// ---------------------------------------------------------------------------

TEST ("algorithm: each event knows which node it came from (its parent in the tree)")
{
    const auto result = engine::buildSchedule (branching());
    CHECK (result.ok());
    const auto& s = *result.schedule;

    CHECK (eventOf (s, 1)->from == model::noNode);
    CHECK (eventOf (s, 2)->from == 1);
    CHECK (eventOf (s, 3)->from == 1);
    CHECK (eventOf (s, 4)->from == 2);
    CHECK (eventOf (s, 5)->from == 3);
}

TEST ("simultaneous siblings: the group shares the parent as source")
{
    auto t = branching();
    t.siblings = model::SiblingMode::Simultaneous;

    const auto result = engine::buildSchedule (t);
    CHECK (result.ok());
    const auto& s = *result.schedule;

    CHECK (eventOf (s, 2)->from == 1);
    CHECK (eventOf (s, 3)->from == 1);
}

TEST ("timeline: the source is the connected neighbour that sounded just before")
{
    model::Track t;
    t.directed = false;
    t.nodes = { node (1, 0, 0), node (2, 100, 0), node (3, 200, 50), node (4, 300, 0) };
    t.edges = { edge (1, 2), edge (2, 3), edge (1, 4), edge (3, 4) };
    t.startNode = 1;
    t.endNode = 4;

    const auto result = engine::buildSchedule (t);
    CHECK (result.ok());
    const auto& s = *result.schedule;

    CHECK (eventOf (s, 1)->from == model::noNode);
    CHECK (eventOf (s, 2)->from == 1);
    CHECK (eventOf (s, 3)->from == 2);
    CHECK (eventOf (s, 4)->from == 3);   // 3 sounded after 1
}
