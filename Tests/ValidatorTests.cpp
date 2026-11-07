#include "TestFramework.h"
#include "TestGraphs.h"

#include "engine/Validator.h"

using namespace chimedes;
using test::node;
using test::edge;
using Problem = engine::ValidationResult::Problem;

namespace
{
    // A -> B <- C : when directed, C cannot be reached from A.
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

TEST ("validation: an empty track cannot be played")
{
    model::Track t;
    CHECK (engine::validate (t).problem == Problem::NoNodes);
}

TEST ("validation: the start or end node is missing")
{
    auto t = arrowsIntoB (false);

    t.startNode = model::noNode;
    CHECK (engine::validate (t).problem == Problem::MissingStart);

    t.startNode = 1;
    t.endNode = 99;  // does not exist
    CHECK (engine::validate (t).problem == Problem::MissingEnd);
}

TEST ("validation: when directed, edge direction is followed")
{
    const auto result = engine::validate (arrowsIntoB (true));

    CHECK (result.problem == Problem::Unreachable);
    REQUIRE (result.unreachable.size() == 1);
    CHECK (result.unreachable[0] == 3);
}

TEST ("validation: the same graph undirected is connected")
{
    const auto result = engine::validate (arrowsIntoB (false));

    CHECK (result.ok());
    CHECK (result.unreachable.empty());
}

TEST ("validation: an isolated node is marked unreachable")
{
    auto t = arrowsIntoB (false);
    t.nodes.push_back (node (4, 500, 500));

    const auto result = engine::validate (t);

    CHECK (result.problem == Problem::Unreachable);
    REQUIRE (result.unreachable.size() == 1);
    CHECK (result.unreachable[0] == 4);
}
