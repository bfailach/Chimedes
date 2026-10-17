#pragma once

#include <vector>

#include "model/Track.h"

namespace chimedes::engine
{

struct ValidationResult
{
    enum class Problem
    {
        None,
        NoNodes,
        MissingStart,       // no hay nodo de inicio o no existe
        MissingEnd,         // no hay nodo de fin o no existe
        Unreachable         // algún nodo no es alcanzable desde el inicio
    };

    Problem problem = Problem::None;
    std::vector<model::NodeId> unreachable;  // ordenados por id

    bool ok() const noexcept { return problem == Problem::None; }
};

// Comprueba que la pista se puede reproducir: inicio y fin definidos y todo
// nodo alcanzable desde el inicio, respetando la dirección en grafos dirigidos.
ValidationResult validate (const model::Track& track);

} // namespace chimedes::engine
