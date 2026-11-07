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
    InstrumentId instrument = 0;    // 0 = el de la pista

    bool operator== (const Node&) const = default;
};

struct Edge
{
    EdgeId id = 0;
    NodeId from = noNode;
    NodeId to = noNode;             // en grafos no dirigidos el orden no importa

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

    // Ajuste opcional al pulso en el modo algoritmo: cada espera se redondea a un
    // múltiplo de esta subdivisión (en pulsos). 0 = sin ajuste, esperas libres.
    double quantizeBeats = 0.0;

    InstrumentId instrument = 1;    // ver model/Instruments.h (1 = órgano)
    bool muted = false;

    // Devuelve el nodo con ese id, o nullptr si no existe.
    const Node* findNode (NodeId nodeId) const noexcept;

    // Longitud euclidiana de una arista en píxeles; 0 si algún extremo no existe.
    double edgeLength (const Edge& edge) const noexcept;

    bool operator== (const Track&) const = default;
};

// Una muestra de sonido cargada por el usuario, usada como instrumento: suena
// transpuesta según la nota, con `rootNote` como la nota en que fue grabada.
struct Sample
{
    InstrumentId id = 0;        // 7..16 (ver model/Instruments.h)
    std::string name;           // para mostrar; por defecto, el nombre del archivo
    std::string path;           // en memoria, absoluta; en el archivo, relativa si se puede
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
