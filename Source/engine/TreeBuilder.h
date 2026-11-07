#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "model/Track.h"

namespace chimedes::engine
{

// Tree rooted at the start node. It only contains reachable nodes.
struct Tree
{
    model::NodeId root = model::noNode;

    // Parent of each node (the root is not listed) and length of the edge joining them.
    std::unordered_map<model::NodeId, model::NodeId> parent;
    std::unordered_map<model::NodeId, double> parentEdgeLength;

    // Children of each node, from the shortest edge to the longest (ties by id).
    std::unordered_map<model::NodeId, std::vector<model::NodeId>> children;

    bool contains (model::NodeId node) const noexcept
    {
        return node == root || parent.contains (node);
    }

    const std::vector<model::NodeId>& childrenOf (model::NodeId node) const;
};

// Turns the track's graph into a tree using the chosen method.
// Returns nullopt if there is no start node, or if Prim is requested on a directed
// graph (the minimum spanning tree is only defined for undirected graphs).
std::optional<Tree> buildTree (const model::Track& track, model::TreeMethod method);

} // namespace chimedes::engine
