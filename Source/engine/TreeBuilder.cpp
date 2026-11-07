#include "engine/TreeBuilder.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <tuple>
#include <unordered_set>

#include "engine/Adjacency.h"

namespace chimedes::engine
{

using model::NodeId;

const std::vector<NodeId>& Tree::childrenOf (NodeId node) const
{
    static const std::vector<NodeId> none;
    const auto it = children.find (node);
    return it != children.end() ? it->second : none;
}

namespace
{
    void link (Tree& tree, NodeId parent, NodeId child, double length)
    {
        tree.parent[child] = parent;
        tree.parentEdgeLength[child] = length;
    }

    // Breadth-first spanning tree: each node hangs from the first one that finds it.
    void buildBfs (Tree& tree, const Adjacency& adjacency)
    {
        std::unordered_set<NodeId> seen { tree.root };
        std::queue<NodeId> pending;
        pending.push (tree.root);

        while (! pending.empty())
        {
            const auto current = pending.front();
            pending.pop();

            for (const auto& n : adjacency.at (current))
            {
                if (seen.insert (n.node).second)
                {
                    link (tree, current, n.node, n.length);
                    pending.push (n.node);
                }
            }
        }
    }

    // Depth-first spanning tree, iterative so the stack never overflows.
    void buildDfs (Tree& tree, const Adjacency& adjacency)
    {
        std::unordered_set<NodeId> seen { tree.root };
        std::vector<std::pair<NodeId, std::size_t>> stack { { tree.root, 0 } };

        while (! stack.empty())
        {
            auto& [current, next] = stack.back();
            const auto& neighbors = adjacency.at (current);

            if (next >= neighbors.size())
            {
                stack.pop_back();
                continue;
            }

            const auto n = neighbors[next++];

            if (seen.insert (n.node).second)
            {
                link (tree, current, n.node, n.length);
                stack.push_back ({ n.node, 0 });
            }
        }
    }

    // Shortest-path tree from the root, with edge lengths as weights.
    void buildDijkstra (Tree& tree, const Adjacency& adjacency)
    {
        std::unordered_map<NodeId, double> distance { { tree.root, 0.0 } };
        std::unordered_set<NodeId> settled;

        using Item = std::pair<double, NodeId>;  // (distance, node); ties by id
        std::priority_queue<Item, std::vector<Item>, std::greater<>> pending;
        pending.push ({ 0.0, tree.root });

        while (! pending.empty())
        {
            const auto [dist, current] = pending.top();
            pending.pop();

            if (! settled.insert (current).second)
                continue;

            for (const auto& n : adjacency.at (current))
            {
                if (settled.contains (n.node))
                    continue;

                const auto candidate = dist + n.length;
                const auto it = distance.find (n.node);

                if (it == distance.end() || candidate < it->second)
                {
                    distance[n.node] = candidate;
                    link (tree, current, n.node, n.length);
                    pending.push ({ candidate, n.node });
                }
            }
        }
    }

    // Minimum spanning tree (Prim) from the root. Undirected graphs only.
    void buildPrim (Tree& tree, const Adjacency& adjacency)
    {
        std::unordered_set<NodeId> inTree { tree.root };

        using Item = std::tuple<double, NodeId, NodeId>;  // (length, child, parent)
        std::priority_queue<Item, std::vector<Item>, std::greater<>> pending;

        const auto addEdgesFrom = [&] (NodeId from)
        {
            for (const auto& n : adjacency.at (from))
                if (! inTree.contains (n.node))
                    pending.push ({ n.length, n.node, from });
        };

        addEdgesFrom (tree.root);

        while (! pending.empty())
        {
            const auto [length, child, parent] = pending.top();
            pending.pop();

            if (! inTree.insert (child).second)
                continue;

            link (tree, parent, child, length);
            addEdgesFrom (child);
        }
    }

    void fillChildren (Tree& tree)
    {
        for (const auto& [child, parent] : tree.parent)
            tree.children[parent].push_back (child);

        for (auto& [parent, kids] : tree.children)
        {
            std::sort (kids.begin(), kids.end(), [&tree] (NodeId a, NodeId b)
            {
                const auto la = tree.parentEdgeLength.at (a);
                const auto lb = tree.parentEdgeLength.at (b);
                return la != lb ? la < lb : a < b;
            });
        }
    }
} // namespace

std::optional<Tree> buildTree (const model::Track& track, model::TreeMethod method)
{
    if (track.findNode (track.startNode) == nullptr)
        return std::nullopt;

    if (method == model::TreeMethod::Prim && track.directed)
        return std::nullopt;

    const auto adjacency = buildAdjacency (track);

    Tree tree;
    tree.root = track.startNode;

    switch (method)
    {
        case model::TreeMethod::BFS:      buildBfs (tree, adjacency);      break;
        case model::TreeMethod::DFS:      buildDfs (tree, adjacency);      break;
        case model::TreeMethod::Dijkstra: buildDijkstra (tree, adjacency); break;
        case model::TreeMethod::Prim:     buildPrim (tree, adjacency);     break;
    }

    fillChildren (tree);
    return tree;
}

} // namespace chimedes::engine
