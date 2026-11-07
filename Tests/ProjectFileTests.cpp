#include "TestFramework.h"

#include "model/DemoProject.h"
#include "model/ProjectFile.h"
#include "model/Randomizer.h"

using namespace chimedes;

namespace
{
    model::Project projectWith (const model::Track& track, double bpm = 120.0)
    {
        model::Project p;
        p.bpm = bpm;
        p.tracks = { track };
        return p;
    }
}

TEST ("proyecto: guardar y abrir devuelve exactamente lo mismo")
{
    auto demo = model::makeDemoTrack();
    demo.name = "Órbita \"lenta\"\ncon salto";
    demo.mode = model::PlaybackMode::Algorithm;
    demo.treeMethod = model::TreeMethod::Dijkstra;
    demo.traversal = model::Traversal::DFS;
    demo.siblings = model::SiblingMode::Simultaneous;
    demo.quantizeBeats = 0.25;
    demo.instrument = 4;
    demo.nodes[1].x = 123.456789;   // decimales: deben volver idénticos
    demo.nodes[1].instrument = 2;

    const auto loaded = model::projectFromJson (model::projectToJson (projectWith (demo, 97.5)));
    CHECK (loaded.ok);
    CHECK (loaded.project.bpm == 97.5);
    CHECK (loaded.project.tracks.size() == 1);
    CHECK (loaded.project.tracks[0] == demo);
}

TEST ("proyecto: grafos aleatorios sobreviven a guardar y abrir")
{
    for (std::uint32_t seed = 1; seed <= 50; ++seed)
    {
        model::Track t;
        t.id = 1;
        t.directed = seed % 2 == 0;
        model::randomizeGraph (t, seed);

        const auto loaded = model::projectFromJson (model::projectToJson (projectWith (t)));
        CHECK (loaded.ok);
        CHECK (loaded.project.tracks[0] == t);
    }
}

TEST ("proyecto: los campos que faltan toman su valor por defecto")
{
    const auto loaded = model::projectFromJson (R"({ "format": "chimedes", "tracks": [ { "nodes": [ { "id": 3 } ] } ] })");
    CHECK (loaded.ok);
    CHECK (loaded.project.bpm == 120.0);

    const auto& t = loaded.project.tracks[0];
    CHECK (t.mode == model::PlaybackMode::Timeline);
    CHECK (t.directed);
    CHECK (t.nodes.size() == 1);
    CHECK (t.nodes[0].id == 3);
    CHECK (t.nodes[0].pitches == std::vector<int> { 60 });
    CHECK (t.nodes[0].velocity == 100);
}

TEST ("proyecto: se descartan aristas imposibles y se ajustan valores fuera de rango")
{
    const auto loaded = model::projectFromJson (R"({
        "format": "chimedes", "version": 1, "bpm": 9000,
        "tracks": [ {
            "directed": false, "mode": "inventado", "start": 1, "end": 99,
            "nodes": [ { "id": 1, "pitches": [200, -5, 60, 60], "velocity": 500 },
                       { "id": 2 }, { "id": 2 } ],
            "edges": [ { "from": 1, "to": 2 }, { "from": 2, "to": 1 }, { "from": 1, "to": 1 },
                       { "from": 1, "to": 42 } ]
        } ] })");

    CHECK (loaded.ok);
    CHECK (loaded.project.bpm == 400.0);

    const auto& t = loaded.project.tracks[0];
    CHECK (t.mode == model::PlaybackMode::Timeline);        // palabra desconocida
    CHECK (t.nodes.size() == 3);
    CHECK (t.nodes[0].pitches == (std::vector<int> { 0, 60, 127 }));
    CHECK (t.nodes[0].velocity == 127);
    CHECK (t.nodes[2].id != 2);                             // id repetido: recibe otro
    CHECK (t.edges.size() == 1);                            // la inversa, el bucle y la rota fuera
    CHECK (t.startNode == 1);
    CHECK (t.endNode == model::noNode);                     // el nodo 99 no existe
}

TEST ("proyecto: errores claros al abrir algo que no es un proyecto")
{
    const auto broken = model::projectFromJson ("{\n  \"format\": \"chimedes\",\n  \"bpm\": ,\n}");
    CHECK (! broken.ok);
    CHECK (broken.error.find ("dañado") != std::string::npos);
    CHECK (broken.error.find ("línea 3") != std::string::npos);

    CHECK (! model::projectFromJson (R"({ "bpm": 120 })").ok);
    CHECK (! model::projectFromJson ("hola").ok);
    CHECK (! model::projectFromJson ("").ok);

    const auto newer = model::projectFromJson (R"({ "format": "chimedes", "version": 99 })");
    CHECK (! newer.ok);
    CHECK (newer.error.find ("más nueva") != std::string::npos);
}

TEST ("proyecto: un archivo sin pistas abre con una pista vacía")
{
    const auto loaded = model::projectFromJson (R"({ "format": "chimedes", "tracks": [] })");
    CHECK (loaded.ok);
    CHECK (loaded.project.tracks.size() == 1);
    CHECK (loaded.project.tracks[0].nodes.empty());
}

TEST ("proyecto: varias pistas se guardan y abren en orden, con nombre y silencio")
{
    model::Project p;
    p.bpm = 140.0;

    for (std::uint32_t i = 1; i <= 4; ++i)
    {
        model::Track t;
        t.id = i * 10;
        t.name = "Pista " + std::to_string (i);
        t.instrument = i;
        t.muted = i == 3;
        model::randomizeGraph (t, i);
        p.tracks.push_back (t);
    }

    const auto loaded = model::projectFromJson (model::projectToJson (p));
    CHECK (loaded.ok);
    CHECK (loaded.project.tracks.size() == 4);
    CHECK (loaded.project.tracks == p.tracks);
    CHECK (loaded.project.tracks[2].muted);
}
