#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace chimedes::model
{

// Identificadores numéricos; 0 significa "ninguno".
using NodeId       = std::uint32_t;
using EdgeId       = std::uint32_t;
using TrackId      = std::uint32_t;
using InstrumentId = std::uint32_t;

inline constexpr NodeId noNode = 0;

struct Node
{
    NodeId id = noNode;
    double x = 0.0;                 // píxeles; x / 100 = pulso
    double y = 0.0;
    std::vector<int> pitches;       // notas MIDI; una = nota, varias = acorde
    double durationBeats = 1.0;
    int velocity = 100;             // 1..127
    InstrumentId instrument = 0;    // 0 = el de la pista (fase 3)
};

struct Edge
{
    EdgeId id = 0;
    NodeId from = noNode;
    NodeId to = noNode;             // en grafos no dirigidos el orden no importa
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

    InstrumentId instrument = 0;
    bool muted = false;

    // Devuelve el nodo con ese id, o nullptr si no existe.
    const Node* findNode (NodeId nodeId) const noexcept;

    // Longitud euclidiana de una arista en píxeles; 0 si algún extremo no existe.
    double edgeLength (const Edge& edge) const noexcept;
};

struct Project
{
    int formatVersion = 1;
    double bpm = 120.0;
    std::vector<Track> tracks;
};

} // namespace chimedes::model
