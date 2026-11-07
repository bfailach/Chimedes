#include "engine/Adjacency.h"

#include <algorithm>

namespace chimedes::engine
{

Adjacency buildAdjacency (const model::Track& track)
{
    Adjacency adjacency;

    for (const auto& node : track.nodes)
        adjacency[node.id];  // every node gets an entry, even without neighbours

    for (const auto& edge : track.edges)
    {
        if (track.findNode (edge.from) == nullptr || track.findNode (edge.to) == nullptr)
            continue;

        const auto length = track.edgeLength (edge);
        adjacency[edge.from].push_back ({ edge.to, length });

        if (! track.directed)
            adjacency[edge.to].push_back ({ edge.from, length });
    }

    for (auto& [id, neighbors] : adjacency)
    {
        std::sort (neighbors.begin(), neighbors.end(), [] (const Neighbor& a, const Neighbor& b)
        {
            return a.length != b.length ? a.length < b.length : a.node < b.node;
        });
    }

    return adjacency;
}

} // namespace chimedes::engine
