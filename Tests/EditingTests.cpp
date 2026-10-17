#include "TestFramework.h"
#include "TestGraphs.h"

#include "model/NoteNames.h"
#include "model/TrackEditing.h"

using namespace chimedes;

// ---------------------------------------------------------------------------
// Edición de la pista
// ---------------------------------------------------------------------------

TEST ("edición: los dos primeros nodos son el inicio y el fin")
{
    model::Track t;
    const auto a = model::addNode (t, 0, 0);
    const auto b = model::addNode (t, 100, 0);
    const auto c = model::addNode (t, 200, 0);

    CHECK (t.startNode == a);
    CHECK (t.endNode == b);
    CHECK (a != b && b != c && a != c);
}

TEST ("edición: borrar un nodo borra sus aristas y su papel de inicio")
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

    CHECK (! model::removeNode (t, a));  // ya no existe
}

TEST ("edición: un nodo nuevo ocupa el inicio que quedó libre")
{
    model::Track t;
    const auto a = model::addNode (t, 0, 0);
    model::addNode (t, 100, 0);
    model::removeNode (t, a);

    const auto c = model::addNode (t, 50, 0);
    CHECK (t.startNode == c);
}

TEST ("edición: no se repiten aristas ni se conecta un nodo consigo mismo")
{
    model::Track t;
    t.directed = true;
    const auto a = model::addNode (t, 0, 0);
    const auto b = model::addNode (t, 100, 0);

    CHECK (model::addEdge (t, a, b).has_value());
    CHECK (! model::addEdge (t, a, b).has_value());   // repetida
    CHECK (! model::addEdge (t, a, a).has_value());   // bucle
    CHECK (! model::addEdge (t, a, 99).has_value());  // nodo inexistente
    CHECK (model::addEdge (t, b, a).has_value());     // en dirigido, el sentido contrario es otra arista

    model::Track u;
    u.directed = false;
    const auto c = model::addNode (u, 0, 0);
    const auto d = model::addNode (u, 100, 0);
    CHECK (model::addEdge (u, c, d).has_value());
    CHECK (! model::addEdge (u, d, c).has_value());   // en no dirigido es la misma
}

TEST ("edición: mover y borrar aristas")
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
// Nombres de notas
// ---------------------------------------------------------------------------

TEST ("notas: nombres con octava, alteraciones y números MIDI")
{
    CHECK (model::parsePitch ("C4") == 60);
    CHECK (model::parsePitch ("c4") == 60);
    CHECK (model::parsePitch ("A4") == 69);
    CHECK (model::parsePitch ("F#3") == 54);
    CHECK (model::parsePitch ("Bb2") == 46);
    CHECK (model::parsePitch ("C-1") == 0);
    CHECK (model::parsePitch ("E") == 64);   // octava 4 por defecto
    CHECK (model::parsePitch ("72") == 72);
}

TEST ("notas: textos inválidos se rechazan")
{
    CHECK (! model::parsePitch ("").has_value());
    CHECK (! model::parsePitch ("H4").has_value());
    CHECK (! model::parsePitch ("C4x").has_value());
    CHECK (! model::parsePitch ("200").has_value());
    CHECK (! model::parsePitch ("G9#").has_value());
}

TEST ("notas: acordes separados por espacios o comas")
{
    const auto chord = model::parsePitches ("C4 E4, G4");
    REQUIRE (chord.has_value());
    CHECK ((*chord == std::vector<int> { 60, 64, 67 }));

    CHECK (! model::parsePitches ("").has_value());
    CHECK (! model::parsePitches ("C4 X9").has_value());
}

TEST ("notas: de número a nombre y de vuelta")
{
    CHECK (model::pitchName (60) == "C4");
    CHECK (model::pitchName (61) == "C#4");
    CHECK (model::pitchName (57) == "A3");
    CHECK (model::pitchNames ({ 60, 64, 67 }) == "C4 E4 G4");

    for (int p = 0; p <= 127; ++p)
        CHECK (model::parsePitch (model::pitchName (p)) == p);
}
