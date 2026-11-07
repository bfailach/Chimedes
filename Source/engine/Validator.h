#pragma once

#include <vector>

#include "model/Track.h"

namespace chimedes::engine
{

struct ValidationResult
{
    enum class Problem
    {
        None,
        NoNodes,
        MissingStart,       // no start node, or it does not exist
        MissingEnd,         // no end node, or it does not exist
        Unreachable         // some node cannot be reached from the start
    };

    Problem problem = Problem::None;
    std::vector<model::NodeId> unreachable;  // sorted by id

    bool ok() const noexcept { return problem == Problem::None; }
};

// Checks that the track can be played: start and end set, and every node
// reachable from the start, following edge direction in directed graphs.
ValidationResult validate (const model::Track& track);

} // namespace chimedes::engine
