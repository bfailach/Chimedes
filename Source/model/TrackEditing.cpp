#include "model/TrackEditing.h"

#include <algorithm>

namespace chimedes::model
{

namespace
{
    template <typename Items>
    std::uint32_t nextId (const Items& items)
    {
        std::uint32_t highest = 0;
        for (const auto& item : items)
            highest = std::max (highest, item.id);
        return highest + 1;
    }
}

Node* findNodeMutable (Track& track, NodeId id) noexcept
{
    const auto it = std::find_if (track.nodes.begin(), track.nodes.end(),
                                  [id] (const Node& n) { return n.id == id; });
    return it != track.nodes.end() ? &*it : nullptr;
}

NodeId addNode (Track& track, double x, double y, std::vector<int> pitches, double durationBeats)
{
    Node node;
    node.id = nextId (track.nodes);
    node.x = x;
    node.y = y;
    node.pitches = std::move (pitches);
    node.durationBeats = durationBeats;
    track.nodes.push_back (node);

    if (track.findNode (track.startNode) == nullptr)
        track.startNode = node.id;
    else if (track.findNode (track.endNode) == nullptr)
        track.endNode = node.id;

    return node.id;
}

bool removeNode (Track& track, NodeId id)
{
    const auto before = track.nodes.size();
    std::erase_if (track.nodes, [id] (const Node& n) { return n.id == id; });

    if (track.nodes.size() == before)
        return false;

    std::erase_if (track.edges, [id] (const Edge& e) { return e.from == id || e.to == id; });

    if (track.startNode == id) track.startNode = noNode;
    if (track.endNode == id)   track.endNode = noNode;

    return true;
}

bool moveNode (Track& track, NodeId id, double x, double y)
{
    auto* node = findNodeMutable (track, id);
    if (node == nullptr)
        return false;

    node->x = x;
    node->y = y;
    return true;
}

std::optional<EdgeId> addEdge (Track& track, NodeId from, NodeId to)
{
    if (from == to || track.findNode (from) == nullptr || track.findNode (to) == nullptr)
        return std::nullopt;

    const auto exists = std::any_of (track.edges.begin(), track.edges.end(), [&] (const Edge& e)
    {
        const bool same = e.from == from && e.to == to;
        const bool reversed = e.from == to && e.to == from;
        return same || (! track.directed && reversed);
    });

    if (exists)
        return std::nullopt;

    const auto id = nextId (track.edges);
    track.edges.push_back ({ id, from, to });
    return id;
}

bool removeEdge (Track& track, EdgeId id)
{
    return std::erase_if (track.edges, [id] (const Edge& e) { return e.id == id; }) > 0;
}

} // namespace chimedes::model
