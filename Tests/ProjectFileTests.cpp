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

TEST ("project: saving and opening gives back exactly the same")
{
    auto demo = model::makeDemoTrack();
    demo.name = "Slow \"orbit\"\nwith a break and ñ";
    demo.mode = model::PlaybackMode::Algorithm;
    demo.treeMethod = model::TreeMethod::Dijkstra;
    demo.traversal = model::Traversal::DFS;
    demo.siblings = model::SiblingMode::Simultaneous;
    demo.quantizeBeats = 0.25;
    demo.instrument = 4;
    demo.nodes[1].x = 123.456789;   // decimals: must come back identical
    demo.nodes[1].instrument = 2;

    const auto loaded = model::projectFromJson (model::projectToJson (projectWith (demo, 97.5)));
    CHECK (loaded.ok);
    CHECK (loaded.project.bpm == 97.5);
    CHECK (loaded.project.tracks.size() == 1);
    CHECK (loaded.project.tracks[0] == demo);
}

TEST ("project: random graphs survive saving and opening")
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

TEST ("project: missing fields take their default value")
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

TEST ("project: impossible edges are dropped and out-of-range values clamped")
{
    const auto loaded = model::projectFromJson (R"({
        "format": "chimedes", "version": 1, "bpm": 9000,
        "tracks": [ {
            "directed": false, "mode": "made-up", "start": 1, "end": 99,
            "nodes": [ { "id": 1, "pitches": [200, -5, 60, 60], "velocity": 500 },
                       { "id": 2 }, { "id": 2 } ],
            "edges": [ { "from": 1, "to": 2 }, { "from": 2, "to": 1 }, { "from": 1, "to": 1 },
                       { "from": 1, "to": 42 } ]
        } ] })");

    CHECK (loaded.ok);
    CHECK (loaded.project.bpm == 400.0);

    const auto& t = loaded.project.tracks[0];
    CHECK (t.mode == model::PlaybackMode::Timeline);        // unknown word
    CHECK (t.nodes.size() == 3);
    CHECK (t.nodes[0].pitches == (std::vector<int> { 0, 60, 127 }));
    CHECK (t.nodes[0].velocity == 127);
    CHECK (t.nodes[2].id != 2);                             // repeated id: gets another
    CHECK (t.edges.size() == 1);                            // the reverse, the loop and the broken one out
    CHECK (t.startNode == 1);
    CHECK (t.endNode == model::noNode);                     // node 99 does not exist
}

TEST ("project: clear errors when opening something that is not a project")
{
    const auto broken = model::projectFromJson ("{\n  \"format\": \"chimedes\",\n  \"bpm\": ,\n}");
    CHECK (! broken.ok);
    CHECK (broken.error.find ("damaged") != std::string::npos);
    CHECK (broken.error.find ("line 3") != std::string::npos);

    CHECK (! model::projectFromJson (R"({ "bpm": 120 })").ok);
    CHECK (! model::projectFromJson ("hello").ok);
    CHECK (! model::projectFromJson ("").ok);

    const auto newer = model::projectFromJson (R"({ "format": "chimedes", "version": 99 })");
    CHECK (! newer.ok);
    CHECK (newer.error.find ("newer") != std::string::npos);
}

TEST ("project: a file without tracks opens with one empty track")
{
    const auto loaded = model::projectFromJson (R"({ "format": "chimedes", "tracks": [] })");
    CHECK (loaded.ok);
    CHECK (loaded.project.tracks.size() == 1);
    CHECK (loaded.project.tracks[0].nodes.empty());
}

TEST ("project: several tracks are saved and opened in order, with name and mute")
{
    model::Project p;
    p.bpm = 140.0;

    for (std::uint32_t i = 1; i <= 4; ++i)
    {
        model::Track t;
        t.id = i * 10;
        t.name = "Track " + std::to_string (i);
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

TEST ("project: samples are saved and opened; invalid ones are dropped")
{
    model::Project p;
    p.tracks = { model::makeDemoTrack() };
    p.tracks[0].instrument = 7;
    p.samples = { { 7, "Soft \"kalimba\"", "sounds/kalimba.wav", 62 },
                  { 9, "Voice", "C:\\Users\\bryan\\voice.wav", 60 } };

    const auto loaded = model::projectFromJson (model::projectToJson (p));
    CHECK (loaded.ok);
    CHECK (loaded.project.samples == p.samples);
    CHECK (loaded.project.tracks[0].instrument == 7);

    const auto messy = model::projectFromJson (R"({ "format": "chimedes", "samples": [
        { "id": 3, "path": "a.wav" }, { "id": 8, "path": "" }, { "id": 8, "path": "b.wav", "root": 300 },
        { "id": 8, "path": "c.wav" } ] })");
    CHECK (messy.ok);
    CHECK (messy.project.samples.size() == 1);   // id 3 is built-in, no path, repeated
    CHECK (messy.project.samples[0].path == "b.wav");
    CHECK (messy.project.samples[0].rootNote == 127);
}

TEST ("project: without samples, the file has no samples section")
{
    model::Project p;
    p.tracks = { model::makeDemoTrack() };
    CHECK (model::projectToJson (p).find ("samples") == std::string::npos);
}
