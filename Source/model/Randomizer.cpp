#include "model/Randomizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>

#include "model/Units.h"

namespace chimedes::model
{

namespace
{
    enum class Shape { Chain, Branches, Fan, Web };

    // Scales in semitones from the tonic.
    const std::array<std::vector<int>, 5> scales {
        std::vector<int> { 0, 2, 4, 7, 9 },             // major pentatonic
        std::vector<int> { 0, 3, 5, 7, 10 },            // minor pentatonic
        std::vector<int> { 0, 2, 4, 5, 7, 9, 11 },      // major
        std::vector<int> { 0, 2, 3, 5, 7, 9, 10 },      // dorian
        std::vector<int> { 0, 2, 3, 5, 7, 8, 10 },      // natural minor
    };

    constexpr std::array<double, 5> spansInBeats { 4.0, 6.0, 8.0, 12.0, 16.0 };
    constexpr std::array<double, 5> noteDurations { 0.5, 1.0, 1.0, 1.5, 2.0 };
    constexpr std::array<double, 4> endDurations { 1.0, 1.0, 2.0, 2.0 };

    class Dice
    {
    public:
        explicit Dice (std::uint32_t seed) : engine (seed) {}

        int between (int lo, int hi) { return std::uniform_int_distribution<int> (lo, hi) (engine); }
        double between (double lo, double hi) { return std::uniform_real_distribution<double> (lo, hi) (engine); }
        bool chance (double p) { return between (0.0, 1.0) < p; }

        template <typename T, std::size_t N>
        T pick (const std::array<T, N>& items) { return items[static_cast<std::size_t> (between (0, static_cast<int> (N) - 1))]; }

    private:
        std::mt19937 engine;
    };

    // MIDI note of scale degree `degree` (it can be negative or go past the octave).
    int pitchOf (const std::vector<int>& scale, int root, int degree)
    {
        const auto size = static_cast<int> (scale.size());
        const auto octave = static_cast<int> (std::floor (static_cast<double> (degree) / size));
        const auto step = degree - octave * size;
        return std::clamp (60 + root + 12 * octave + scale[static_cast<std::size_t> (step)], 24, 108);
    }
}

void randomizeGraph (Track& track, std::uint32_t seed)
{
    Dice dice (seed);

    const auto count = dice.between (5, 14);
    const auto shape = static_cast<Shape> (dice.between (0, 3));
    // Random overall length, with at least half a beat per note (counting the last)
    // so they all fit on the grid without overlapping.
    std::vector<double> spans;
    for (const auto beats : spansInBeats)
        if (beats >= 0.5 * count)
            spans.push_back (beats);
    const auto spanBeats = spans[static_cast<std::size_t> (dice.between (0, static_cast<int> (spans.size()) - 1))];
    const auto verticalSpread = dice.between (120.0, 420.0);   // closeness
    const auto clustering = dice.between (0.6, 1.7);           // < 1: bunched at the end; > 1: at the start

    const auto& scale = scales[static_cast<std::size_t> (dice.between (0, static_cast<int> (scales.size()) - 1))];
    const auto root = dice.between (0, 11);

    // --- Rhythm: notes land on a grid of beats (or half beats) and the last one
    // ends exactly when the chosen length is over. So the cycle lasts a whole number
    // of beats and the music follows the BPM instead of sounding off the beat. ---
    auto endDuration = dice.pick (endDurations);
    // Leave room for every note: with many, the end gets shorter (half a beat minimum).
    endDuration = std::max (0.5, std::min (endDuration, spanBeats - 0.5 * (count - 1)));
    const auto usableBeats = spanBeats - endDuration;
    auto grid = dice.chance (0.6) ? 0.5 : 1.0;
    if (usableBeats / grid + 1.0 < count)
        grid = 0.5;

    // Positions spread from 0 to the end, a little uneven, snapped to the grid.
    std::vector<double> beats (static_cast<std::size_t> (count));
    for (int i = 0; i < count; ++i)
    {
        auto u = static_cast<double> (i) / (count - 1);
        if (i > 0 && i < count - 1)
            u = std::clamp (u + dice.between (-0.35, 0.35) / (count - 1), 0.02, 0.98);
        beats[static_cast<std::size_t> (i)] = std::round (std::pow (u, clustering) * usableBeats / grid) * grid;
    }
    std::sort (beats.begin(), beats.end());

    // No two notes share a position: at least one grid step between neighbours,
    // with the first at 0 and the last at the end.
    beats.front() = 0.0;
    beats.back() = usableBeats;
    for (std::size_t i = 1; i + 1 < beats.size(); ++i)
        beats[i] = std::max (beats[i], beats[i - 1] + grid);
    for (std::size_t i = beats.size() - 1; i-- > 0;)
        beats[i] = std::min (beats[i], beats[i + 1] - grid);

    std::vector<double> xs;
    for (const auto b : beats)
        xs.push_back (beatsToPixels (b));

    // --- Nodes ---
    std::vector<Node> nodes;
    std::vector<int> degrees;  // scale degree of each node, to chain melodies

    for (int i = 0; i < count; ++i)
    {
        Node n;
        n.id = static_cast<NodeId> (i + 1);
        n.x = xs[static_cast<std::size_t> (i)];
        n.y = std::round (shape == Shape::Chain && i > 0
                              ? std::clamp (nodes.back().y + dice.between (-0.35, 0.35) * verticalSpread, 40.0, 40.0 + verticalSpread)
                              : 40.0 + dice.between (0.0, verticalSpread));
        n.durationBeats = dice.pick (noteDurations);
        n.velocity = dice.between (70, 115);
        nodes.push_back (n);
        degrees.push_back (0);
    }

    // --- Edges: each node hangs from an earlier one, depending on the shape ---
    std::vector<Edge> edges;
    EdgeId nextEdge = 1;

    const auto connect = [&] (int from, int to)
    {
        const auto a = nodes[static_cast<std::size_t> (from)].id;
        const auto b = nodes[static_cast<std::size_t> (to)].id;
        const auto exists = std::any_of (edges.begin(), edges.end(), [&] (const Edge& e)
        {
            return (e.from == a && e.to == b) || (e.from == b && e.to == a);
        });
        if (! exists)
            edges.push_back ({ nextEdge++, a, b });
    };

    for (int i = 1; i < count; ++i)
    {
        int parent = i - 1;

        switch (shape)
        {
            case Shape::Chain:    parent = i - 1;                                  break;
            case Shape::Branches: parent = dice.between (std::max (0, i - 3), i - 1); break;
            case Shape::Fan:      parent = dice.between (0, std::min (1, i - 1));  break;
            case Shape::Web:      parent = dice.between (std::max (0, i - 3), i - 1); break;
        }

        connect (parent, i);

        // The melody moves through the scale from the parent node's degree.
        degrees[static_cast<std::size_t> (i)] = std::clamp (degrees[static_cast<std::size_t> (parent)] + dice.between (-2, 2), -3, 10);
    }

    // The web adds extra connections, always to the right.
    if (shape == Shape::Web)
        for (int i = 2; i < count; ++i)
            if (dice.chance (0.35))
                connect (dice.between (0, i - 2), i);

    // --- Notes: one note of the scale, or sometimes a three-note chord ---
    for (int i = 0; i < count; ++i)
    {
        const auto d = degrees[static_cast<std::size_t> (i)];
        auto& n = nodes[static_cast<std::size_t> (i)];

        if (dice.chance (0.15))
            n.pitches = { pitchOf (scale, root, d), pitchOf (scale, root, d + 2), pitchOf (scale, root, d + 4) };
        else
            n.pitches = { pitchOf (scale, root, d) };
    }

    // The last node closes the phrase on the tonic and lasts until the end of the cycle.
    nodes.back().pitches = { pitchOf (scale, root, 0) };
    nodes.back().durationBeats = endDuration;

    track.nodes = std::move (nodes);
    track.edges = std::move (edges);
    track.startNode = track.nodes.front().id;
    track.endNode = track.nodes.back().id;
}

// ---------------------------------------------------------------------------
// Randomize one thing only, keeping the graph
// ---------------------------------------------------------------------------

namespace
{
    // Derived seeds: each aspect uses its own dice, so "everything" with one seed
    // does not repeat the same rolls at each step.
    std::uint32_t mix (std::uint32_t seed, std::uint32_t salt)
    {
        auto h = seed ^ (salt * 0x9e3779b9u);
        h ^= h >> 16;  h *= 0x85ebca6bu;  h ^= h >> 13;
        return h;
    }

    // Order in which the graph is visited from the start (breadth-first, ignoring
    // edge direction) and which node each one is reached from. Loose nodes go last.
    std::vector<std::pair<NodeId, NodeId>> walkFromStart (const Track& track)
    {
        std::vector<std::pair<NodeId, NodeId>> order;   // (node, from)
        std::vector<NodeId> seen;
        const auto visited = [&seen] (NodeId id) { return std::find (seen.begin(), seen.end(), id) != seen.end(); };

        auto start = track.findNode (track.startNode) != nullptr ? track.startNode
                   : track.nodes.empty() ? noNode : track.nodes.front().id;

        const auto visitFrom = [&] (NodeId root)
        {
            order.push_back ({ root, noNode });
            seen.push_back (root);
            for (std::size_t i = order.size() - 1; i < order.size(); ++i)
                for (const auto& e : track.edges)
                {
                    const auto here = order[i].first;
                    const auto other = e.from == here ? e.to : e.to == here ? e.from : noNode;
                    if (other != noNode && ! visited (other))
                    {
                        seen.push_back (other);
                        order.push_back ({ other, here });
                    }
                }
        };

        if (start != noNode)
            visitFrom (start);
        for (const auto& n : track.nodes)
            if (! visited (n.id))
                visitFrom (n.id);

        return order;
    }
}

void randomizeNotes (Track& track, std::uint32_t seed)
{
    if (track.nodes.empty())
        return;

    Dice dice (seed);
    const auto& scale = scales[static_cast<std::size_t> (dice.between (0, static_cast<int> (scales.size()) - 1))];
    const auto root = dice.between (0, 11);
    const auto chordChance = dice.between (0.05, 0.3);

    std::vector<std::pair<NodeId, int>> degreeOf;
    const auto degree = [&degreeOf] (NodeId id)
    {
        for (const auto& [n, d] : degreeOf)
            if (n == id)
                return d;
        return 0;
    };

    for (const auto& [id, from] : walkFromStart (track))
    {
        const auto d = from == noNode ? dice.between (-1, 2) : std::clamp (degree (from) + dice.between (-2, 2), -4, 11);
        degreeOf.push_back ({ id, d });

        auto* n = &*std::find_if (track.nodes.begin(), track.nodes.end(), [id] (const Node& x) { return x.id == id; });
        if (dice.chance (chordChance))
            n->pitches = { pitchOf (scale, root, d), pitchOf (scale, root, d + 2), pitchOf (scale, root, d + 4) };
        else
            n->pitches = { pitchOf (scale, root, d) };
    }

    // The end resolves to the tonic.
    for (auto& n : track.nodes)
        if (n.id == track.endNode)
            n.pitches = { pitchOf (scale, root, 0) };
}

void randomizeRhythm (Track& track, std::uint32_t seed)
{
    Dice dice (seed);

    // One character for the whole track: shorter or longer, more even or more accented.
    const auto longer = dice.between (0.0, 1.0);
    const auto baseVelocity = dice.between (60, 100);
    const auto accent = dice.between (0, 35);

    constexpr std::array<double, 6> durations { 0.25, 0.5, 0.5, 1.0, 1.5, 2.0 };

    for (auto& n : track.nodes)
    {
        if (n.id != track.endNode)
        {
            // Biased towards long or short notes depending on the character.
            const auto u = std::pow (dice.between (0.0, 1.0), 1.5 - longer);
            n.durationBeats = durations[std::min (durations.size() - 1, static_cast<std::size_t> (u * durations.size()))];
        }

        n.velocity = std::clamp (baseVelocity + (dice.chance (0.3) ? accent : 0) + dice.between (-10, 10), 30, 127);
    }
}

void randomizeInstruments (Track& track, std::uint32_t seed, const std::vector<InstrumentId>& available)
{
    if (available.empty())
        return;

    Dice dice (seed);
    const auto pick = [&] { return available[static_cast<std::size_t> (dice.between (0, static_cast<int> (available.size()) - 1))]; };

    track.instrument = pick();

    // Sometimes a few nodes with another instrument (a different colour in the phrase).
    const auto accents = available.size() > 1 && dice.chance (0.5);
    for (auto& n : track.nodes)
    {
        n.instrument = 0;   // same as the track
        if (accents && dice.chance (0.2))
        {
            auto other = pick();
            while (other == track.instrument)
                other = pick();
            n.instrument = other;
        }
    }
}

void randomizeAlgorithm (Track& track, std::uint32_t seed)
{
    Dice dice (seed);

    track.treeMethod = static_cast<TreeMethod> (dice.between (0, track.directed ? 2 : 3));   // Prim only when undirected
    track.traversal = dice.chance (0.5) ? Traversal::BFS : Traversal::DFS;
    track.siblings = dice.chance (0.5) ? SiblingMode::Sequential : SiblingMode::Simultaneous;

    constexpr std::array<double, 4> steps { 0.0, 1.0, 0.5, 0.25 };
    track.quantizeBeats = dice.chance (0.75) ? steps[static_cast<std::size_t> (dice.between (1, 3))] : 0.0;   // almost always on the beat
}

void randomizeEverything (Track& track, std::uint32_t seed, const std::vector<InstrumentId>& available)
{
    randomizeGraph (track, mix (seed, 1));
    randomizeRhythm (track, mix (seed, 2));
    randomizeInstruments (track, mix (seed, 3), available);
    randomizeAlgorithm (track, mix (seed, 4));
}

} // namespace chimedes::model
