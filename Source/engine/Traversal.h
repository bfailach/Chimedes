#pragma once

#include <vector>

#include "engine/TreeBuilder.h"

namespace chimedes::engine
{

// Orden en que el recorrido elegido visita el árbol, empezando por la raíz.
// Los hijos se visitan de la arista más corta a la más larga.
std::vector<model::NodeId> visitOrder (const Tree& tree, model::Traversal traversal);

} // namespace chimedes::engine
