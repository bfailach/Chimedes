#include "engine/Validator.h"

#include <algorithm>
#include <queue>
#include <unordered_set>

#include "engine/Adjacency.h"

namespace chimedes::engine
{

ValidationResult validate (const model::Track& track)
{
    using Problem = ValidationResult::Problem;
    ValidationResult result;

    if (track.nodes.empty())
    {
        result.problem = Problem::NoNodes;
        return result;
    }

    if (track.findNode (track.startNode) == nullptr)
    {
        result.problem = Problem::MissingStart;
        return result;
    }

    if (track.findNode (track.endNode) == nullptr)
    {
        result.problem = Problem::MissingEnd;
        return result;
    }

    const auto adjacency = buildAdjacency (track);

    std::unordered_set<model::NodeId> reached { track.startNode };
    std::queue<model::NodeId> pending;
    pending.push (track.startNode);

    while (! pending.empty())
    {
        const auto current = pending.front();
        pending.pop();

        for (const auto& neighbor : adjacency.at (current))
            if (reached.insert (neighbor.node).second)
                pending.push (neighbor.node);
    }

    for (const auto& node : track.nodes)
        if (! reached.contains (node.id))
            result.unreachable.push_back (node.id);

    if (! result.unreachable.empty())
    {
        std::sort (result.unreachable.begin(), result.unreachable.end());
        result.problem = Problem::Unreachable;
    }

    return result;
}

} // namespace chimedes::engine
