#include "model/Track.h"

#include <algorithm>
#include <cmath>

namespace chimedes::model
{

const Node* Track::findNode (NodeId nodeId) const noexcept
{
    const auto it = std::find_if (nodes.begin(), nodes.end(),
                                  [nodeId] (const Node& n) { return n.id == nodeId; });
    return it != nodes.end() ? &*it : nullptr;
}

double Track::edgeLength (const Edge& edge) const noexcept
{
    const auto* a = findNode (edge.from);
    const auto* b = findNode (edge.to);

    if (a == nullptr || b == nullptr)
        return 0.0;

    return std::hypot (b->x - a->x, b->y - a->y);
}

} // namespace chimedes::model
