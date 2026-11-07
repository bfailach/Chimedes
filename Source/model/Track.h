#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace chimedes::model
{

// Numeric identifiers; 0 means "none".
using NodeId       = std::uint32_t;
using EdgeId       = std::uint32_t;
using TrackId      = std::uint32_t;
using InstrumentId = std::uint32_t;

inline constexpr NodeId noNode = 0;

struct Node
{
    NodeId id = noNode;
    double x = 0.0;                 // pixels; x / 100 = beat
    double y = 0.0;
    std::vector<int> pitches;       // MIDI notes; one = note, several = chord
    double durationBeats = 1.0;
    int velocity = 100;             // 1..127
    InstrumentId instrument = 0;    // 0 = the track's

    bool operator== (const Node&) const = default;
};

struct Edge
{
    EdgeId id = 0;
    NodeId from = noNode;
    NodeId to = noNode;             // in undirected graphs the order does not matter

    bool operator== (const Edge&) const = default;
};

enum class PlaybackMode { Timeline, Algorithm };
enum class TreeMethod   { BFS, DFS, Dijkstra, Prim };
enum class Traversal    { BFS, DFS };
enum class SiblingMode  { Sequential, Simultaneous };

struct Track
{
    TrackId id = 0;
    std::string name;
    bool directed = true;

    std::vector<Node> nodes;
    std::vector<Edge> edges;

    NodeId startNode = noNode;
    NodeId endNode = noNode;

    PlaybackMode mode = PlaybackMode::Timeline;
    TreeMethod treeMethod = TreeMethod::BFS;
    Traversal traversal = Traversal::BFS;
    SiblingMode siblings = SiblingMode::Sequential;

    // Optional beat snapping in algorithm mode: each wait is rounded to a multiple
    // of this subdivision (in beats). 0 = no snapping, free waits.
    double quantizeBeats = 0.0;

    InstrumentId instrument = 17;   // see model/Instruments.h (17 = piano)
    bool muted = false;

    // Returns the node with that id, or nullptr if it does not exist.
    const Node* findNode (NodeId nodeId) const noexcept;

    // Euclidean length of an edge in pixels; 0 if either end does not exist.
    double edgeLength (const Edge& edge) const noexcept;

    bool operator== (const Track&) const = default;
};

// A sound sample loaded by the user and used as an instrument: it plays
// transposed per note, with `rootNote` as the note it was recorded at.
struct Sample
{
    InstrumentId id = 0;        // 7..16 (see model/Instruments.h)
    std::string name;           // for display; defaults to the file name
    std::string path;           // absolute in memory; relative in the file when possible
    int rootNote = 60;

    bool operator== (const Sample&) const = default;
};

struct Project
{
    int formatVersion = 1;
    double bpm = 120.0;
    std::vector<Track> tracks;
    std::vector<Sample> samples;

    bool operator== (const Project&) const = default;
};

} // namespace chimedes::model
