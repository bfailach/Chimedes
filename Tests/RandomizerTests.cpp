#include "TestFramework.h"

#include <algorithm>

#include "engine/Scheduler.h"
#include "model/Randomizer.h"

using namespace chimedes;

TEST ("aleatorio: el grafo siempre se puede reproducir, en todos los modos")
{
    int failures = 0;

    for (std::uint32_t seed = 1; seed <= 300; ++seed)
    {
        for (const bool directed : { true, false })
        {
            model::Track t;
            t.directed = directed;
            model::randomizeGraph (t, seed);

            for (const auto mode : { model::PlaybackMode::Timeline, model::PlaybackMode::Algorithm })
            {
                for (const auto method : { model::TreeMethod::BFS, model::TreeMethod::DFS, model::TreeMethod::Dijkstra })
                {
                    t.mode = mode;
                    t.treeMethod = method;
                    failures += engine::buildSchedule (t).ok() ? 0 : 1;
                }
            }
        }
    }

    CHECK (failures == 0);
}

TEST ("aleatorio: cantidad de nodos, inicio a la izquierda y fin a la derecha")
{
    for (std::uint32_t seed = 1; seed <= 200; ++seed)
    {
        model::Track t;
        model::randomizeGraph (t, seed);

        CHECK (t.nodes.size() >= 5 && t.nodes.size() <= 14);

        const auto [minIt, maxIt] = std::minmax_element (t.nodes.begin(), t.nodes.end(),
            [] (const model::Node& a, const model::Node& b) { return a.x < b.x; });

        CHECK (t.findNode (t.startNode)->x == minIt->x);
        CHECK (t.findNode (t.endNode)->x == maxIt->x);
        CHECK (minIt->x >= 0.0);

        // La duración general es una de las previstas (de inicio a fin, en pulsos).
        const auto span = (maxIt->x - minIt->x) / 100.0;
        CHECK (span == 4.0 || span == 6.0 || span == 8.0 || span == 12.0 || span == 16.0);

        for (const auto& n : t.nodes)
        {
            CHECK (! n.pitches.empty());
            for (const auto p : n.pitches)
                CHECK (p >= 0 && p <= 127);
        }
    }
}

TEST ("aleatorio: la misma semilla da el mismo grafo y otra semilla, otro")
{
    model::Track a, b, c;
    model::randomizeGraph (a, 1234);
    model::randomizeGraph (b, 1234);
    model::randomizeGraph (c, 1235);

    CHECK (a == b);
    CHECK (! (a == c));
}

TEST ("aleatorio: conserva las opciones de la pista")
{
    model::Track t;
    t.mode = model::PlaybackMode::Algorithm;
    t.instrument = 3;
    t.quantizeBeats = 0.5;
    t.directed = false;

    model::randomizeGraph (t, 7);

    CHECK (t.mode == model::PlaybackMode::Algorithm);
    CHECK (t.instrument == 3);
    CHECK (t.quantizeBeats == 0.5);
    CHECK (! t.directed);
}

TEST ("aleatorio: aparecen las cuatro formas y acordes")
{
    // Señales indirectas de variedad en 200 semillas: cadenas (nadie con más de un
    // hijo), grafos con ramas, grafos con más aristas que nodos - 1 (red) y acordes.
    int chains = 0, branched = 0, webs = 0, withChords = 0;

    for (std::uint32_t seed = 1; seed <= 200; ++seed)
    {
        model::Track t;
        model::randomizeGraph (t, seed);

        std::vector<int> children (t.nodes.size() + 1, 0);
        for (const auto& e : t.edges)
            ++children[e.from];

        const auto maxChildren = *std::max_element (children.begin(), children.end());
        chains += maxChildren == 1 ? 1 : 0;
        branched += maxChildren > 1 ? 1 : 0;
        webs += t.edges.size() > t.nodes.size() - 1 ? 1 : 0;
        withChords += std::any_of (t.nodes.begin(), t.nodes.end(), [] (const model::Node& n) { return n.pitches.size() > 1; }) ? 1 : 0;
    }

    CHECK (chains > 10);
    CHECK (branched > 10);
    CHECK (webs > 10);
    CHECK (withChords > 10);
}
