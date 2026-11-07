#pragma once

#include "model/Track.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace chimedes::model
{

// Demo graph shown at startup: a short phrase in C major with two branches
// that meet in a final chord. Directed, start at 1, end at 8.
inline Track makeDemoTrack()
{
    Track t;
    t.id = 1;
    t.name = "Demo";
    t.directed = true;

    const auto add = [&t] (NodeId id, double x, double y, std::vector<int> pitches, double duration)
    {
        Node n;
        n.id = id;
        n.x = x;
        n.y = y;
        n.pitches = std::move (pitches);
        n.durationBeats = duration;
        t.nodes.push_back (std::move (n));
    };

    add (1,   0, 260, { 60 },         1.0);  // C
    add (2, 100, 160, { 64 },         1.0);  // E
    add (3, 150, 380, { 67 },         1.0);  // G
    add (4, 250, 120, { 69 },         1.0);  // A
    add (5, 300, 280, { 60, 64, 67 }, 2.0);  // C major
    add (6, 400, 420, { 74 },         1.0);  // D
    add (7, 450, 180, { 76 },         1.0);  // high E
    add (8, 600, 280, { 57, 60, 64 }, 2.0);  // A minor

    EdgeId nextEdge = 1;
    for (const auto& [from, to] : std::vector<std::pair<NodeId, NodeId>> {
             { 1, 2 }, { 1, 3 }, { 2, 4 }, { 3, 5 }, { 4, 7 }, { 5, 6 }, { 6, 8 }, { 7, 8 } })
        t.edges.push_back ({ nextEdge++, from, to });

    t.startNode = 1;
    t.endNode = 8;
    return t;
}

// The piece Chimedes opens with: "Carousel", an original 16-bar waltz in D minor
// for kalimba. Every beat is a star on a ring, like a carousel seen from above:
// the start is at the top and the music turns clockwise, one beat per edge, back
// to the start. Each star sits further out the higher its top note, so the ring
// rises and falls with the melody like the horses. Played in algorithm mode with
// beat snapping, so those small bumps never change the rhythm.
inline Track makeCarouselTrack()
{
    struct Beat
    {
        std::vector<int> pitches;
        double duration;
        int velocity;
    };

    // Bass on the downbeat with the melody; the two other beats of each bar
    // ("oom-pah-pah") carry the chord, plus the melody when it moves.
    const std::vector<Beat> score {
        // A section
        { { 50, 69 },         2.0, 100 }, { { 62, 65 },     1.0, 66 }, { { 62, 65, 74 }, 1.0, 92 },  // Dm
        { { 48, 77 },         2.0, 100 }, { { 62, 65 },     1.0, 66 }, { { 62, 65, 76 }, 1.0, 92 },  // Dm/C
        { { 46, 74 },         2.0, 100 }, { { 62, 65 },     1.0, 66 }, { { 62, 65, 70 }, 1.0, 92 },  // Bb
        { { 45, 73 },         3.0, 100 }, { { 61, 64 },     1.0, 66 }, { { 61, 64 },     1.0, 66 },  // A
        { { 43, 70 },         2.0, 100 }, { { 58, 62 },     1.0, 66 }, { { 58, 62, 74 }, 1.0, 92 },  // Gm
        { { 53, 69 },         1.0, 100 }, { { 57, 62, 65 }, 1.0, 88 }, { { 57, 62, 69 }, 1.0, 92 },  // Dm/F
        { { 52, 67 },         2.0, 100 }, { { 58, 62 },     1.0, 66 }, { { 58, 62, 70 }, 1.0, 92 },  // Em7b5
        { { 45, 69 },         3.0, 100 }, { { 61, 67 },     1.0, 66 }, { { 61, 67 },     1.0, 66 },  // A7
        // B section
        { { 53, 72 },         2.0, 100 }, { { 57, 60 },     1.0, 66 }, { { 57, 60, 77 }, 1.0, 92 },  // F
        { { 52, 76 },         1.0, 100 }, { { 55, 60, 79 }, 1.0, 92 }, { { 55, 60, 76 }, 1.0, 88 },  // C/E
        { { 50, 77 },         2.0, 100 }, { { 57, 62 },     1.0, 66 }, { { 57, 62, 74 }, 1.0, 92 },  // Dm
        { { 46, 74 },         1.0, 100 }, { { 62, 65, 72 }, 1.0, 88 }, { { 62, 65, 70 }, 1.0, 88 },  // Bb
        { { 43, 70 },         2.0, 100 }, { { 58, 62 },     1.0, 66 }, { { 58, 62, 67 }, 1.0, 92 },  // Gm
        { { 45, 76 },         2.0, 100 }, { { 61, 64 },     1.0, 66 }, { { 61, 64, 73 }, 1.0, 92 },  // A
        { { 50, 74 },         3.0, 104 }, { { 57, 62 },     1.0, 66 }, { { 57, 62 },     1.0, 66 },  // Dm
        { { 38, 50, 57, 62 }, 2.0, 96 },  { { 57, 62, 65 }, 1.0, 62 }, { { 64 },         1.0, 70 },  // Dm, lead back
    };

    Track t;
    t.id = 1;
    t.name = "Carousel";
    t.directed = true;
    t.mode = PlaybackMode::Algorithm;
    t.treeMethod = TreeMethod::BFS;
    t.traversal = Traversal::BFS;
    t.quantizeBeats = 1.0;
    t.instrument = 22;  // kalimba

    // A regular polygon whose sides are one beat long.
    constexpr double pi = 3.14159265358979323846;
    const auto count = static_cast<double> (score.size());
    const double radius = 100.0 / (2.0 * std::sin (pi / count));
    const double centre = radius + 120.0;

    for (std::size_t i = 0; i < score.size(); ++i)
    {
        const auto& beat = score[i];
        const auto top = *std::max_element (beat.pitches.begin(), beat.pitches.end());
        const auto lift = std::clamp ((top - 66) * 4.0, -24.0, 52.0);
        const auto angle = -pi / 2.0 + 2.0 * pi * static_cast<double> (i) / count;

        Node n;
        n.id = static_cast<NodeId> (i + 1);
        n.x = std::round (centre + (radius + lift) * std::cos (angle));
        n.y = std::round (centre + (radius + lift) * std::sin (angle));
        n.pitches = beat.pitches;
        n.durationBeats = beat.duration;
        n.velocity = beat.velocity;
        t.nodes.push_back (std::move (n));
    }

    // Around the ring, and the last beat closes it back to the start.
    for (std::size_t i = 0; i < score.size(); ++i)
        t.edges.push_back ({ static_cast<EdgeId> (i + 1),
                             static_cast<NodeId> (i + 1),
                             static_cast<NodeId> ((i + 1) % score.size() + 1) });

    t.startNode = 1;
    t.endNode = static_cast<NodeId> (score.size());
    return t;
}

} // namespace chimedes::model
