#pragma once

#include <optional>
#include <vector>

#include "model/Track.h"

// Editing operations on a track. They are pure functions on the model: the UI
// calls them inside an undo action and then recomputes the schedule.
namespace chimedes::model
{

Node* findNodeMutable (Track& track, NodeId id) noexcept;

// Creates a node and returns its id. If the track has no start node, the new node
// becomes it; if it has a start but no end, it becomes the end. So a new track can
// be played as soon as it has two connected nodes.
NodeId addNode (Track& track, double x, double y,
                std::vector<int> pitches = { 60 }, double durationBeats = 1.0);

// Deletes a node and its edges. If it was the start or the end, the track loses it.
bool removeNode (Track& track, NodeId id);

bool moveNode (Track& track, NodeId id, double x, double y);

// Connects two nodes. Returns nullopt if either does not exist, if they are the same
// node or if the edge already exists (in undirected graphs, also the other way round).
std::optional<EdgeId> addEdge (Track& track, NodeId from, NodeId to);

bool removeEdge (Track& track, EdgeId id);

} // namespace chimedes::model
