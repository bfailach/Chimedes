#include "TestFramework.h"

#include <algorithm>
#include <cmath>

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

        // La duración general es una de las previstas: del inicio hasta que termina la
        // última nota, en pulsos.
        const auto span = (maxIt->x - minIt->x) / 100.0 + t.findNode (t.endNode)->durationBeats;
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

TEST ("aleatorio: las notas caen en medios pulsos y el ciclo dura pulsos enteros")
{
    // Si las notas cayeran entre pulsos, la música no seguiría al BPM.
    for (std::uint32_t seed = 1; seed <= 300; ++seed)
    {
        model::Track t;
        model::randomizeGraph (t, seed);

        const auto result = engine::buildSchedule (t);
        CHECK (result.ok());
        const auto& s = *result.schedule;

        for (const auto& e : s.events)
            CHECK (std::abs (e.startBeat * 2.0 - std::round (e.startBeat * 2.0)) < 1e-9);

        CHECK (std::abs (s.cycleLengthBeats - std::round (s.cycleLengthBeats)) < 1e-9);

        // Ninguna nota comparte posición con otra.
        for (std::size_t i = 1; i < s.events.size(); ++i)
            CHECK (s.events[i].startBeat > s.events[i - 1].startBeat);
    }
}

// ---------------------------------------------------------------------------
// Aleatorizar una sola cosa
// ---------------------------------------------------------------------------

namespace
{
    // Igual forma: mismos nodos (id y posición) y mismas aristas.
    bool sameShape (const model::Track& a, const model::Track& b)
    {
        if (a.nodes.size() != b.nodes.size() || ! (a.edges == b.edges) || a.startNode != b.startNode || a.endNode != b.endNode)
            return false;
        for (std::size_t i = 0; i < a.nodes.size(); ++i)
            if (a.nodes[i].id != b.nodes[i].id || a.nodes[i].x != b.nodes[i].x || a.nodes[i].y != b.nodes[i].y)
                return false;
        return true;
    }

    model::Track randomTrack (std::uint32_t seed, bool directed = true)
    {
        model::Track t;
        t.id = 1;
        t.directed = directed;
        model::randomizeGraph (t, seed);
        return t;
    }
}

TEST ("aleatorio de notas: cambia las notas, conserva la forma y cierra en la tónica")
{
    for (std::uint32_t seed = 1; seed <= 100; ++seed)
    {
        const auto before = randomTrack (seed);
        auto t = before;
        model::randomizeNotes (t, seed + 1000);

        CHECK (sameShape (before, t));
        for (const auto& n : t.nodes)
        {
            CHECK (! n.pitches.empty());
            for (const auto p : n.pitches)
                CHECK (p >= 24 && p <= 108);
        }
        CHECK (t.findNode (t.endNode)->pitches.size() == 1);
        CHECK (engine::buildSchedule (t).ok());
    }

    // Misma semilla, mismo resultado.
    auto a = randomTrack (7), b = randomTrack (7);
    model::randomizeNotes (a, 99);
    model::randomizeNotes (b, 99);
    CHECK (a == b);
}

TEST ("aleatorio de ritmo: conserva la forma y la duración del fin")
{
    for (std::uint32_t seed = 1; seed <= 100; ++seed)
    {
        const auto before = randomTrack (seed);
        auto t = before;
        model::randomizeRhythm (t, seed * 3);

        CHECK (sameShape (before, t));
        CHECK (t.findNode (t.endNode)->durationBeats == before.findNode (before.endNode)->durationBeats);
        for (const auto& n : t.nodes)
        {
            CHECK (n.durationBeats >= 0.25 && n.durationBeats <= 2.0);
            CHECK (n.velocity >= 30 && n.velocity <= 127);
        }

        // En línea de tiempo el ciclo no cambia de largo.
        CHECK (engine::buildSchedule (t).schedule->cycleLengthBeats == engine::buildSchedule (before).schedule->cycleLengthBeats);
    }
}

TEST ("aleatorio de instrumentos: solo usa los disponibles")
{
    const std::vector<model::InstrumentId> available { 2, 5, 7 };
    for (std::uint32_t seed = 1; seed <= 100; ++seed)
    {
        auto t = randomTrack (seed);
        model::randomizeInstruments (t, seed, available);

        CHECK (std::find (available.begin(), available.end(), t.instrument) != available.end());
        for (const auto& n : t.nodes)
            CHECK (n.instrument == 0 || (std::find (available.begin(), available.end(), n.instrument) != available.end()
                                         && n.instrument != t.instrument));
    }

    auto t = randomTrack (3);
    const auto before = t;
    model::randomizeInstruments (t, 3, {});
    CHECK (t == before);
}

TEST ("aleatorio de algoritmo: Prim solo en grafos no dirigidos y siempre se puede reproducir")
{
    for (std::uint32_t seed = 1; seed <= 200; ++seed)
    {
        for (const bool directed : { true, false })
        {
            auto t = randomTrack (seed, directed);
            t.mode = model::PlaybackMode::Algorithm;
            model::randomizeAlgorithm (t, seed);

            CHECK (t.mode == model::PlaybackMode::Algorithm);
            if (directed)
                CHECK (t.treeMethod != model::TreeMethod::Prim);
            CHECK (engine::buildSchedule (t).ok());
        }
    }
}

TEST ("aleatorio de todo: siempre se puede reproducir y respeta las opciones de la pista")
{
    const std::vector<model::InstrumentId> available { 1, 2, 3, 4, 5, 6 };
    for (std::uint32_t seed = 1; seed <= 200; ++seed)
    {
        model::Track t;
        t.id = 4;
        t.name = "Bajo";
        t.directed = seed % 2 == 0;
        t.mode = seed % 3 == 0 ? model::PlaybackMode::Algorithm : model::PlaybackMode::Timeline;
        const auto before = t;

        model::randomizeEverything (t, seed, available);

        CHECK (engine::buildSchedule (t).ok());
        CHECK (t.id == before.id && t.name == before.name && t.directed == before.directed && t.mode == before.mode);
    }
}
