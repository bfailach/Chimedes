#include "TestFramework.h"
#include "TestGraphs.h"

#include "engine/Validator.h"

using namespace chimedes;
using test::node;
using test::edge;
using Problem = engine::ValidationResult::Problem;

namespace
{
    // A -> B <- C : en dirigido, C no es alcanzable desde A.
    model::Track arrowsIntoB (bool directed)
    {
        model::Track t;
        t.directed = directed;
        t.nodes = { node (1, 0, 0), node (2, 100, 0), node (3, 200, 0) };
        t.edges = { edge (1, 2), edge (3, 2) };
        t.startNode = 1;
        t.endNode = 2;
        return t;
    }
}

TEST ("validación: una pista vacía no se puede reproducir")
{
    model::Track t;
    CHECK (engine::validate (t).problem == Problem::NoNodes);
}

TEST ("validación: falta el nodo de inicio o el de fin")
{
    auto t = arrowsIntoB (false);

    t.startNode = model::noNode;
    CHECK (engine::validate (t).problem == Problem::MissingStart);

    t.startNode = 1;
    t.endNode = 99;  // no existe
    CHECK (engine::validate (t).problem == Problem::MissingEnd);
}

TEST ("validación: en dirigido se respeta la dirección de las aristas")
{
    const auto result = engine::validate (arrowsIntoB (true));

    CHECK (result.problem == Problem::Unreachable);
    REQUIRE (result.unreachable.size() == 1);
    CHECK (result.unreachable[0] == 3);
}

TEST ("validación: el mismo grafo no dirigido es conexo")
{
    const auto result = engine::validate (arrowsIntoB (false));

    CHECK (result.ok());
    CHECK (result.unreachable.empty());
}

TEST ("validación: un nodo aislado se marca como inalcanzable")
{
    auto t = arrowsIntoB (false);
    t.nodes.push_back (node (4, 500, 500));

    const auto result = engine::validate (t);

    CHECK (result.problem == Problem::Unreachable);
    REQUIRE (result.unreachable.size() == 1);
    CHECK (result.unreachable[0] == 4);
}
