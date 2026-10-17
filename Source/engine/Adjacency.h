#pragma once

#include <unordered_map>
#include <vector>

#include "model/Track.h"

namespace chimedes::engine
{

struct Neighbor
{
    model::NodeId node = model::noNode;
    double length = 0.0;            // píxeles
};

// Lista de vecinos de cada nodo. En grafos dirigidos solo sigue la dirección
// de las aristas; en no dirigidos, ambas. Cada lista está ordenada de la arista
// más corta a la más larga (empates por id), para que todo sea determinista.
using Adjacency = std::unordered_map<model::NodeId, std::vector<Neighbor>>;

Adjacency buildAdjacency (const model::Track& track);

} // namespace chimedes::engine
