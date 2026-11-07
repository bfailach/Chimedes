#include "TestFramework.h"
#include "TestGraphs.h"

#include "model/NoteNames.h"
#include "model/TrackEditing.h"

using namespace chimedes;

// ---------------------------------------------------------------------------
// Track editing
// ---------------------------------------------------------------------------

TEST ("editing: the first two nodes are the start and the end")
{
    model::Track t;
    const auto a = model::addNode (t, 0, 0);
    const auto b = model::addNode (t, 100, 0);
    const auto c = model::addNode (t, 200, 0);

    CHECK (t.startNode == a);
    CHECK (t.endNode == b);
    CHECK (a != b && b != c && a != c);
}

TEST ("editing: deleting a node deletes its edges and its start role")
{
    model::Track t;
    const auto a = model::addNode (t, 0, 0);
    const auto b = model::addNode (t, 100, 0);
    const auto c = model::addNode (t, 200, 0);
    model::addEdge (t, a, b);
    model::addEdge (t, b, c);

    CHECK (model::removeNode (t, a));
    CHECK (t.nodes.size() == 2);
    CHECK (t.edges.size() == 1);
    CHECK (t.startNode == model::noNode);
    CHECK (t.endNode == b);

    CHECK (! model::removeNode (t, a));  // no longer exists
}

TEST ("editing: a new node takes the start slot left free")
{
    model::Track t;
    const auto a = model::addNode (t, 0, 0);
    model::addNode (t, 100, 0);
    model::removeNode (t, a);

    const auto c = model::addNode (t, 50, 0);
    CHECK (t.startNode == c);
}

TEST ("editing: no duplicate edges and no node connected to itself")
{
    model::Track t;
    t.directed = true;
    const auto a = model::addNode (t, 0, 0);
    const auto b = model::addNode (t, 100, 0);

    CHECK (model::addEdge (t, a, b).has_value());
    CHECK (! model::addEdge (t, a, b).has_value());   // duplicate
    CHECK (! model::addEdge (t, a, a).has_value());   // loop
    CHECK (! model::addEdge (t, a, 99).has_value());  // missing node
    CHECK (model::addEdge (t, b, a).has_value());     // when directed, the reverse is a different edge

    model::Track u;
    u.directed = false;
    const auto c = model::addNode (u, 0, 0);
    const auto d = model::addNode (u, 100, 0);
    CHECK (model::addEdge (u, c, d).has_value());
    CHECK (! model::addEdge (u, d, c).has_value());   // when undirected it is the same one
}

TEST ("editing: moving and deleting edges")
{
    model::Track t;
    const auto a = model::addNode (t, 0, 0);
    const auto b = model::addNode (t, 100, 0);
    const auto e = model::addEdge (t, a, b);
    REQUIRE (e.has_value());

    CHECK (model::moveNode (t, b, 300, 40));
    CHECK_NEAR (t.findNode (b)->x, 300.0);
    CHECK_NEAR (t.findNode (b)->y, 40.0);

    CHECK (model::removeEdge (t, *e));
    CHECK (t.edges.empty());
    CHECK (! model::removeEdge (t, *e));
}

// ---------------------------------------------------------------------------
// Note names
// ---------------------------------------------------------------------------

TEST ("notes: names with octave, accidentals and MIDI numbers")
{
    CHECK (model::parsePitch ("C4") == 60);
    CHECK (model::parsePitch ("c4") == 60);
    CHECK (model::parsePitch ("A4") == 69);
    CHECK (model::parsePitch ("F#3") == 54);
    CHECK (model::parsePitch ("Bb2") == 46);
    CHECK (model::parsePitch ("C-1") == 0);
    CHECK (model::parsePitch ("E") == 64);   // octave 4 by default
    CHECK (model::parsePitch ("72") == 72);
}

TEST ("notes: invalid text is rejected")
{
    CHECK (! model::parsePitch ("").has_value());
    CHECK (! model::parsePitch ("H4").has_value());
    CHECK (! model::parsePitch ("C4x").has_value());
    CHECK (! model::parsePitch ("200").has_value());
    CHECK (! model::parsePitch ("G9#").has_value());
}

TEST ("notes: chords separated by spaces or commas")
{
    const auto chord = model::parsePitches ("C4 E4, G4");
    REQUIRE (chord.has_value());
    CHECK ((*chord == std::vector<int> { 60, 64, 67 }));

    CHECK (! model::parsePitches ("").has_value());
    CHECK (! model::parsePitches ("C4 X9").has_value());
}

TEST ("notes: from number to name and back")
{
    CHECK (model::pitchName (60) == "C4");
    CHECK (model::pitchName (61) == "C#4");
    CHECK (model::pitchName (57) == "A3");
    CHECK (model::pitchNames ({ 60, 64, 67 }) == "C4 E4 G4");

    for (int p = 0; p <= 127; ++p)
        CHECK (model::parsePitch (model::pitchName (p)) == p);
}
