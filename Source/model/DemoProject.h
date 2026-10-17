#pragma once

#include "model/Track.h"

namespace chimedes::model
{

// Grafo de demostración mientras no existe el editor: una pequeña frase en do mayor
// con dos ramas que se reúnen en un acorde final. Dirigido, inicio en 1, fin en 8.
inline Track makeDemoTrack()
{
    Track t;
    t.id = 1;
    t.name = "Demostración";
    t.directed = true;

    const auto add = [&t] (NodeId id, double x, double y, std::vector<int> pitches, double duration)
    {
        Node n;
        n.id = id;
        n.x = x;
        n.y = y;
        n.pitches = std::move (pitches);
        n.durationBeats = duration;
        t.nodes.push_back (std::move (n));
    };

    add (1,   0, 260, { 60 },         1.0);  // do
    add (2, 100, 160, { 64 },         1.0);  // mi
    add (3, 150, 380, { 67 },         1.0);  // sol
    add (4, 250, 120, { 69 },         1.0);  // la
    add (5, 300, 280, { 60, 64, 67 }, 2.0);  // do mayor
    add (6, 400, 420, { 74 },         1.0);  // re
    add (7, 450, 180, { 76 },         1.0);  // mi agudo
    add (8, 600, 280, { 57, 60, 64 }, 2.0);  // la menor

    EdgeId nextEdge = 1;
    for (const auto& [from, to] : std::vector<std::pair<NodeId, NodeId>> {
             { 1, 2 }, { 1, 3 }, { 2, 4 }, { 3, 5 }, { 4, 7 }, { 5, 6 }, { 6, 8 }, { 7, 8 } })
        t.edges.push_back ({ nextEdge++, from, to });

    t.startNode = 1;
    t.endNode = 8;
    return t;
}

} // namespace chimedes::model
