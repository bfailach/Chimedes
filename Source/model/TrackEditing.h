#pragma once

#include <optional>
#include <vector>

#include "model/Track.h"

// Operaciones de edición sobre una pista. Son funciones puras sobre el modelo: la
// interfaz las llama dentro de una acción de deshacer y luego recalcula el horario.
namespace chimedes::model
{

Node* findNodeMutable (Track& track, NodeId id) noexcept;

// Crea un nodo y devuelve su id. Si la pista no tiene nodo de inicio, el nuevo nodo
// lo será; si ya tiene inicio pero no fin, será el fin. Así una pista nueva se puede
// reproducir en cuanto tiene dos nodos conectados.
NodeId addNode (Track& track, double x, double y,
                std::vector<int> pitches = { 60 }, double durationBeats = 1.0);

// Elimina un nodo y sus aristas. Si era el inicio o el fin, la pista se queda sin él.
bool removeNode (Track& track, NodeId id);

bool moveNode (Track& track, NodeId id, double x, double y);

// Conecta dos nodos. Devuelve nullopt si alguno no existe, si es el mismo nodo o si
// la arista ya existe (en no dirigidos, también en sentido contrario).
std::optional<EdgeId> addEdge (Track& track, NodeId from, NodeId to);

bool removeEdge (Track& track, EdgeId id);

} // namespace chimedes::model
