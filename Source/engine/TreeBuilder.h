#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "model/Track.h"

namespace chimedes::engine
{

// Árbol con raíz en el nodo de inicio. Solo contiene los nodos alcanzables.
struct Tree
{
    model::NodeId root = model::noNode;

    // Padre de cada nodo (la raíz no aparece) y longitud de la arista que los une.
    std::unordered_map<model::NodeId, model::NodeId> parent;
    std::unordered_map<model::NodeId, double> parentEdgeLength;

    // Hijos de cada nodo, de la arista más corta a la más larga (empates por id).
    std::unordered_map<model::NodeId, std::vector<model::NodeId>> children;

    bool contains (model::NodeId node) const noexcept
    {
        return node == root || parent.contains (node);
    }

    const std::vector<model::NodeId>& childrenOf (model::NodeId node) const;
};

// Convierte el grafo de la pista en un árbol según el método elegido.
// Devuelve nullopt si no hay nodo de inicio, o si se pide Prim en un grafo
// dirigido (el árbol de expansión mínima solo está definido para no dirigidos).
std::optional<Tree> buildTree (const model::Track& track, model::TreeMethod method);

} // namespace chimedes::engine
