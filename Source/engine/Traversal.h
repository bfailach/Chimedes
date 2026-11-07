#pragma once

#include <vector>

#include "engine/TreeBuilder.h"

namespace chimedes::engine
{

// Order in which the chosen traversal visits the tree, starting at the root.
// Children are visited from the shortest edge to the longest.
std::vector<model::NodeId> visitOrder (const Tree& tree, model::Traversal traversal);

} // namespace chimedes::engine
