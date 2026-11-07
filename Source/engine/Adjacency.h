#pragma once

#include <unordered_map>
#include <vector>

#include "model/Track.h"

namespace chimedes::engine
{

struct Neighbor
{
    model::NodeId node = model::noNode;
    double length = 0.0;            // pixels
};

// Neighbour list for each node. In directed graphs it only follows the edge
// direction; in undirected graphs, both. Each list is sorted from the shortest
// edge to the longest (ties by id), so everything is deterministic.
using Adjacency = std::unordered_map<model::NodeId, std::vector<Neighbor>>;

Adjacency buildAdjacency (const model::Track& track);

} // namespace chimedes::engine
