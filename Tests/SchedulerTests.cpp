#include "TestFramework.h"
#include "TestGraphs.h"

#include "engine/Scheduler.h"

using namespace chimedes;
using test::node;
using test::edge;
using Error = engine::ScheduleResult::Error;

namespace
{
    // Busca el evento de un nodo; nullptr si ese nodo no suena.
    const engine::NoteEvent* eventOf (const engine::Schedule& s, model::NodeId id)
    {
        for (const auto& e : s.events)
            if (e.node == id)
                return &e;
        return nullptr;
    }

    // Un árbol con longitudes redondas (100 px = 1 pulso):
    //
    //            A(0,0)
    //       100 ╱      ╲ 200
    //      B(100,0)    C(0,200)
    //   200 │            │ 300
    //      D(300,0)    E(0,500)
    //
    // BFS visita A, B, C, D, E.   DFS visita A, B, D, C, E.
    model::Track branching (model::NodeId end = 5)
    {
        model::Track t;
        t.directed = true;
        t.mode = model::PlaybackMode::Algorithm;
        t.treeMethod = model::TreeMethod::BFS;
        t.nodes = { node (1, 0, 0), node (2, 100, 0), node (3, 0, 200),
                    node (4, 300, 0), node (5, 0, 500) };
        t.edges = { edge (1, 2), edge (1, 3), edge (2, 4), edge (3, 5) };
        t.startNode = 1;
        t.endNode = end;
        return t;
    }
}

// ---------------------------------------------------------------------------
// Modo línea de tiempo
// ---------------------------------------------------------------------------

TEST ("línea de tiempo: cada nodo suena en x / 100 desde el inicio")
{
    model::Track t;
    t.directed = false;
    t.nodes = { node (1, 100, 0), node (2, 250, 80), node (3, 400, 0, 2.0),
                node (4, 50, 0), node (5, 500, 0) };
    t.edges = { edge (4, 1), edge (1, 2), edge (2, 3), edge (3, 5) };
    t.startNode = 1;
    t.endNode = 3;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    const auto& s = *result.schedule;

    REQUIRE (s.events.size() == 3);
    CHECK_NEAR (eventOf (s, 1)->startBeat, 0.0);
    CHECK_NEAR (eventOf (s, 2)->startBeat, 1.5);
    CHECK_NEAR (eventOf (s, 3)->startBeat, 3.0);

    // Los nodos fuera del tramo inicio–fin no suenan.
    CHECK (eventOf (s, 4) == nullptr);
    CHECK (eventOf (s, 5) == nullptr);

    // El ciclo termina cuando acaba de sonar el nodo de fin (3 + 2 pulsos).
    CHECK_NEAR (s.cycleLengthBeats, 5.0);
}

TEST ("línea de tiempo: el fin a la izquierda del inicio es un error")
{
    model::Track t;
    t.directed = false;
    t.nodes = { node (1, 300, 0), node (2, 100, 0) };
    t.edges = { edge (1, 2) };
    t.startNode = 1;
    t.endNode = 2;

    const auto result = engine::buildSchedule (t);
    CHECK (result.error == Error::EndBeforeStart);
    CHECK (! result.schedule.has_value());
}

TEST ("horario: una pista inválida no genera horario")
{
    auto t = branching();
    t.nodes.push_back (node (9, 900, 900));  // aislado

    const auto result = engine::buildSchedule (t);
    CHECK (result.error == Error::Invalid);
    CHECK (result.validation.unreachable.size() == 1);
    CHECK (! result.schedule.has_value());
}

// ---------------------------------------------------------------------------
// Modo algoritmo
// ---------------------------------------------------------------------------

TEST ("algoritmo secuencial con BFS: espera la arista del padre de cada nodo")
{
    auto t = branching();
    t.traversal = model::Traversal::BFS;
    t.siblings = model::SiblingMode::Sequential;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    const auto& s = *result.schedule;

    REQUIRE (s.events.size() == 5);
    CHECK_NEAR (eventOf (s, 1)->startBeat, 0.0);  // A
    CHECK_NEAR (eventOf (s, 2)->startBeat, 1.0);  // B: + AB 100
    CHECK_NEAR (eventOf (s, 3)->startBeat, 3.0);  // C: + AC 200
    CHECK_NEAR (eventOf (s, 4)->startBeat, 5.0);  // D: + BD 200
    CHECK_NEAR (eventOf (s, 5)->startBeat, 8.0);  // E: + CE 300
    CHECK_NEAR (s.cycleLengthBeats, 9.0);
}

TEST ("algoritmo secuencial con DFS: el mismo grafo suena en otro orden")
{
    auto t = branching();
    t.traversal = model::Traversal::DFS;
    t.siblings = model::SiblingMode::Sequential;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    const auto& s = *result.schedule;

    CHECK_NEAR (eventOf (s, 1)->startBeat, 0.0);  // A
    CHECK_NEAR (eventOf (s, 2)->startBeat, 1.0);  // B
    CHECK_NEAR (eventOf (s, 4)->startBeat, 3.0);  // D antes que C
    CHECK_NEAR (eventOf (s, 3)->startBeat, 5.0);  // C
    CHECK_NEAR (eventOf (s, 5)->startBeat, 8.0);  // E
}

TEST ("algoritmo simultáneo: los hermanos suenan juntos y esperan la arista más corta")
{
    auto t = branching();
    t.traversal = model::Traversal::BFS;
    t.siblings = model::SiblingMode::Simultaneous;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    const auto& s = *result.schedule;

    REQUIRE (s.events.size() == 5);
    CHECK_NEAR (eventOf (s, 1)->startBeat, 0.0);  // A
    CHECK_NEAR (eventOf (s, 2)->startBeat, 1.0);  // B y C juntos: min(100, 200)
    CHECK_NEAR (eventOf (s, 3)->startBeat, 1.0);
    CHECK_NEAR (eventOf (s, 4)->startBeat, 3.0);  // D: + 200
    CHECK_NEAR (eventOf (s, 5)->startBeat, 6.0);  // E: + 300
    CHECK_NEAR (s.cycleLengthBeats, 7.0);
}

TEST ("algoritmo: el ciclo termina en el nodo de fin aunque queden nodos")
{
    auto t = branching (3);  // fin en C
    t.traversal = model::Traversal::BFS;
    t.siblings = model::SiblingMode::Sequential;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());
    const auto& s = *result.schedule;

    CHECK (s.events.size() == 3);         // A, B, C
    CHECK (eventOf (s, 4) == nullptr);    // D y E no llegan a sonar
    CHECK (eventOf (s, 5) == nullptr);
    CHECK_NEAR (s.cycleLengthBeats, 4.0); // C empieza en 3 y dura 1
}

TEST ("algoritmo: inicio y fin en el mismo nodo")
{
    auto t = branching (1);
    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());

    CHECK (result.schedule->events.size() == 1);
    CHECK_NEAR (result.schedule->cycleLengthBeats, 1.0);
}

TEST ("algoritmo: Prim en un grafo dirigido no genera horario")
{
    auto t = branching();
    t.treeMethod = model::TreeMethod::Prim;

    CHECK (engine::buildSchedule (t).error == Error::TreeUnavailable);
}

TEST ("horario: los eventos salen ordenados por tiempo")
{
    auto t = branching();
    t.traversal = model::Traversal::DFS;

    const auto result = engine::buildSchedule (t);
    REQUIRE (result.ok());

    const auto& events = result.schedule->events;
    for (std::size_t i = 1; i < events.size(); ++i)
        CHECK (events[i - 1].startBeat <= events[i].startBeat);
}
