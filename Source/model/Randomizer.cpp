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

    // Escalas en semitonos desde la tónica.
    const std::array<std::vector<int>, 5> scales {
        std::vector<int> { 0, 2, 4, 7, 9 },             // pentatónica mayor
        std::vector<int> { 0, 3, 5, 7, 10 },            // pentatónica menor
        std::vector<int> { 0, 2, 4, 5, 7, 9, 11 },      // mayor
        std::vector<int> { 0, 2, 3, 5, 7, 9, 10 },      // dórica
        std::vector<int> { 0, 2, 3, 5, 7, 8, 10 },      // menor natural
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

    // Nota MIDI del grado `degree` de la escala (puede ser negativo o pasar de la octava).
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
    // Duración general al azar, con al menos medio pulso por nota (contando la última)
    // para que quepan todas en la rejilla sin encimarse.
    std::vector<double> spans;
    for (const auto beats : spansInBeats)
        if (beats >= 0.5 * count)
            spans.push_back (beats);
    const auto spanBeats = spans[static_cast<std::size_t> (dice.between (0, static_cast<int> (spans.size()) - 1))];
    const auto verticalSpread = dice.between (120.0, 420.0);   // cercanía
    const auto clustering = dice.between (0.6, 1.7);           // < 1: apiñados al final; > 1: al principio

    const auto& scale = scales[static_cast<std::size_t> (dice.between (0, static_cast<int> (scales.size()) - 1))];
    const auto root = dice.between (0, 11);

    // --- Ritmo: las notas caen sobre una rejilla de pulsos (o medios pulsos) y la
    // última termina justo al acabar la duración elegida. Así el ciclo dura un número
    // entero de pulsos y la música sigue al BPM en vez de sonar a destiempo. ---
    auto endDuration = dice.pick (endDurations);
    // Deja sitio para todas las notas: con muchas, el final se acorta (mínimo medio pulso).
    endDuration = std::max (0.5, std::min (endDuration, spanBeats - 0.5 * (count - 1)));
    const auto usableBeats = spanBeats - endDuration;
    auto grid = dice.chance (0.6) ? 0.5 : 1.0;
    if (usableBeats / grid + 1.0 < count)
        grid = 0.5;

    // Posiciones repartidas de 0 al final, con algo de irregularidad, ajustadas a la rejilla.
    std::vector<double> beats (static_cast<std::size_t> (count));
    for (int i = 0; i < count; ++i)
    {
        auto u = static_cast<double> (i) / (count - 1);
        if (i > 0 && i < count - 1)
            u = std::clamp (u + dice.between (-0.35, 0.35) / (count - 1), 0.02, 0.98);
        beats[static_cast<std::size_t> (i)] = std::round (std::pow (u, clustering) * usableBeats / grid) * grid;
    }
    std::sort (beats.begin(), beats.end());

    // Ninguna nota comparte posición: al menos un paso de rejilla entre vecinas,
    // con la primera en 0 y la última al final.
    beats.front() = 0.0;
    beats.back() = usableBeats;
    for (std::size_t i = 1; i + 1 < beats.size(); ++i)
        beats[i] = std::max (beats[i], beats[i - 1] + grid);
    for (std::size_t i = beats.size() - 1; i-- > 0;)
        beats[i] = std::min (beats[i], beats[i + 1] - grid);

    std::vector<double> xs;
    for (const auto b : beats)
        xs.push_back (beatsToPixels (b));

    // --- Nodos ---
    std::vector<Node> nodes;
    std::vector<int> degrees;  // grado de la escala de cada nodo, para encadenar melodías

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

    // --- Aristas: cada nodo cuelga de uno anterior, según la forma ---
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

        // La melodía avanza por la escala a partir del grado del nodo padre.
        degrees[static_cast<std::size_t> (i)] = std::clamp (degrees[static_cast<std::size_t> (parent)] + dice.between (-2, 2), -3, 10);
    }

    // La red añade conexiones extra, siempre hacia la derecha.
    if (shape == Shape::Web)
        for (int i = 2; i < count; ++i)
            if (dice.chance (0.35))
                connect (dice.between (0, i - 2), i);

    // --- Notas: una nota de la escala, o a veces un acorde de tres ---
    for (int i = 0; i < count; ++i)
    {
        const auto d = degrees[static_cast<std::size_t> (i)];
        auto& n = nodes[static_cast<std::size_t> (i)];

        if (dice.chance (0.15))
            n.pitches = { pitchOf (scale, root, d), pitchOf (scale, root, d + 2), pitchOf (scale, root, d + 4) };
        else
            n.pitches = { pitchOf (scale, root, d) };
    }

    // El último nodo cierra la frase en la tónica y dura hasta el final del ciclo.
    nodes.back().pitches = { pitchOf (scale, root, 0) };
    nodes.back().durationBeats = endDuration;

    track.nodes = std::move (nodes);
    track.edges = std::move (edges);
    track.startNode = track.nodes.front().id;
    track.endNode = track.nodes.back().id;
}

// ---------------------------------------------------------------------------
// Aleatorizar una sola cosa, conservando el grafo
// ---------------------------------------------------------------------------

namespace
{
    // Semillas derivadas: cada aspecto usa su propio dado, así "todo" con una semilla
    // no repite las mismas tiradas en cada paso.
    std::uint32_t mix (std::uint32_t seed, std::uint32_t salt)
    {
        auto h = seed ^ (salt * 0x9e3779b9u);
        h ^= h >> 16;  h *= 0x85ebca6bu;  h ^= h >> 13;
        return h;
    }

    // Orden en que se visita el grafo desde el inicio (anchura, ignorando el sentido
    // de las aristas) y de qué nodo se llega a cada uno. Los nodos sueltos van al final.
    std::vector<std::pair<NodeId, NodeId>> walkFromStart (const Track& track)
    {
        std::vector<std::pair<NodeId, NodeId>> order;   // (nodo, desde)
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

    // El fin cierra en la tónica.
    for (auto& n : track.nodes)
        if (n.id == track.endNode)
            n.pitches = { pitchOf (scale, root, 0) };
}

void randomizeRhythm (Track& track, std::uint32_t seed)
{
    Dice dice (seed);

    // Un carácter para toda la pista: más cortas o más largas, más parejas o más acentuadas.
    const auto longer = dice.between (0.0, 1.0);
    const auto baseVelocity = dice.between (60, 100);
    const auto accent = dice.between (0, 35);

    constexpr std::array<double, 6> durations { 0.25, 0.5, 0.5, 1.0, 1.5, 2.0 };

    for (auto& n : track.nodes)
    {
        if (n.id != track.endNode)
        {
            // Sesgo hacia las largas o las cortas según el carácter.
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

    // A veces, unos pocos nodos con otro instrumento (un color distinto en la frase).
    const auto accents = available.size() > 1 && dice.chance (0.5);
    for (auto& n : track.nodes)
    {
        n.instrument = 0;   // como la pista
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

    track.treeMethod = static_cast<TreeMethod> (dice.between (0, track.directed ? 2 : 3));   // Prim solo sin dirección
    track.traversal = dice.chance (0.5) ? Traversal::BFS : Traversal::DFS;
    track.siblings = dice.chance (0.5) ? SiblingMode::Sequential : SiblingMode::Simultaneous;

    constexpr std::array<double, 4> steps { 0.0, 1.0, 0.5, 0.25 };
    track.quantizeBeats = dice.chance (0.75) ? steps[static_cast<std::size_t> (dice.between (1, 3))] : 0.0;   // casi siempre a pulso
}

void randomizeEverything (Track& track, std::uint32_t seed, const std::vector<InstrumentId>& available)
{
    randomizeGraph (track, mix (seed, 1));
    randomizeRhythm (track, mix (seed, 2));
    randomizeInstruments (track, mix (seed, 3), available);
    randomizeAlgorithm (track, mix (seed, 4));
}

} // namespace chimedes::model
