#pragma once

#include "model/Track.h"

// Ayudas para construir pistas de prueba de forma legible.
namespace chimedes::test
{

inline model::Node node (model::NodeId id, double x, double y, double duration = 1.0)
{
    model::Node n;
    n.id = id;
    n.x = x;
    n.y = y;
    n.pitches = { 60 };
    n.durationBeats = duration;
    return n;
}

inline model::Edge edge (model::NodeId from, model::NodeId to)
{
    static model::EdgeId nextId = 1;
    return { nextId++, from, to };
}

} // namespace chimedes::test
