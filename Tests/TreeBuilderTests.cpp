#include "TestFramework.h"
#include "TestGraphs.h"

#include "engine/Traversal.h"
#include "engine/TreeBuilder.h"

using namespace chimedes;
using test::node;
using test::edge;
using model::TreeMethod;

namespace
{
    // Un rombo donde cada método da un árbol distinto:
    //
    //   A(0,0) ── B(100,0)        AB = 100
    //   │          │              AC = 300
    //   C(0,300)   │              BD = 400
    //     ╲        │              CD = 141,4
    //      D(100,400)
    //
    // BFS:      D cuelga de B (B se descubre antes que C).
    // DFS:      A → B → D → C, así que C cuelga de D.
    // Dijkstra: D cuelga de C (300 + 141 < 100 + 400).
    // Prim:     D cuelga de C (CD es la arista más corta que lo alcanza).
    model::Track rhombus (bool directed = false)
    {
        model::Track t;
        t.directed = directed;
        t.nodes = { node (1, 0, 0), node (2, 100, 0), node (3, 0, 300), node (4, 100, 400) };
        t.edges = { edge (1, 2), edge (1, 3), edge (2, 4), edge (3, 4) };
        t.startNode = 1;
        t.endNode = 4;
        return t;
    }

    constexpr model::NodeId A = 1, B = 2, C = 3, D = 4;
}

TEST ("árbol BFS: cada nodo cuelga del primero que lo descubre")
{
    const auto tree = engine::buildTree (rhombus(), TreeMethod::BFS);
    REQUIRE (tree.has_value());

    CHECK (tree->root == A);
    CHECK (tree->parent.at (B) == A);
    CHECK (tree->parent.at (C) == A);
    CHECK (tree->parent.at (D) == B);
}

TEST ("árbol DFS: sigue la rama más cercana hasta el fondo")
{
    const auto tree = engine::buildTree (rhombus(), TreeMethod::DFS);
    REQUIRE (tree.has_value());

    CHECK (tree->parent.at (B) == A);
    CHECK (tree->parent.at (D) == B);
    CHECK (tree->parent.at (C) == D);
}

TEST ("árbol Dijkstra: caminos más cortos desde el inicio")
{
    const auto tree = engine::buildTree (rhombus(), TreeMethod::Dijkstra);
    REQUIRE (tree.has_value());

    CHECK (tree->parent.at (B) == A);
    CHECK (tree->parent.at (C) == A);
    CHECK (tree->parent.at (D) == C);
}

TEST ("árbol Prim: expansión mínima en grafos no dirigidos")
{
    const auto tree = engine::buildTree (rhombus(), TreeMethod::Prim);
    REQUIRE (tree.has_value());

    CHECK (tree->parent.at (B) == A);
    CHECK (tree->parent.at (C) == A);
    CHECK (tree->parent.at (D) == C);
}

TEST ("árbol Prim: no aplica a grafos dirigidos")
{
    CHECK (! engine::buildTree (rhombus (true), TreeMethod::Prim).has_value());
}

TEST ("árbol: los hijos van de la arista más corta a la más larga")
{
    const auto tree = engine::buildTree (rhombus(), TreeMethod::BFS);
    REQUIRE (tree.has_value());

    const auto& kids = tree->childrenOf (A);
    REQUIRE (kids.size() == 2);
    CHECK (kids[0] == B);  // 100
    CHECK (kids[1] == C);  // 300
}

TEST ("recorridos: BFS va por capas y DFS por ramas")
{
    const auto tree = engine::buildTree (rhombus(), TreeMethod::Dijkstra);
    REQUIRE (tree.has_value());

    // Árbol: A → {B, C}, C → {D}
    const auto bfs = engine::visitOrder (*tree, model::Traversal::BFS);
    const auto dfs = engine::visitOrder (*tree, model::Traversal::DFS);

    CHECK ((bfs == std::vector<model::NodeId> { A, B, C, D }));
    CHECK ((dfs == std::vector<model::NodeId> { A, B, C, D }));

    const auto chain = engine::buildTree (rhombus(), TreeMethod::BFS);
    REQUIRE (chain.has_value());

    // Árbol: A → {B, C}, B → {D}
    CHECK ((engine::visitOrder (*chain, model::Traversal::BFS) == std::vector<model::NodeId> { A, B, C, D }));
    CHECK ((engine::visitOrder (*chain, model::Traversal::DFS) == std::vector<model::NodeId> { A, B, D, C }));
}
