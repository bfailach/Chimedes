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
    // Duración general al azar, pero con al menos medio pulso por nodo para que no se encimen.
    std::vector<double> spans;
    for (const auto beats : spansInBeats)
        if (beats >= 0.5 * (count - 1))
            spans.push_back (beats);
    const auto spanPixels = beatsToPixels (spans[static_cast<std::size_t> (dice.between (0, static_cast<int> (spans.size()) - 1))]);
    const auto verticalSpread = dice.between (120.0, 420.0);   // cercanía
    const auto clustering = dice.between (0.6, 1.7);           // < 1: apiñados al final; > 1: al principio

    const auto& scale = scales[static_cast<std::size_t> (dice.between (0, static_cast<int> (scales.size()) - 1))];
    const auto root = dice.between (0, 11);

    // --- Posiciones: repartidas de 0 a la duración elegida, con algo de irregularidad ---
    std::vector<double> xs (static_cast<std::size_t> (count));
    for (int i = 0; i < count; ++i)
    {
        auto u = static_cast<double> (i) / (count - 1);
        if (i > 0 && i < count - 1)
            u = std::clamp (u + dice.between (-0.35, 0.35) / (count - 1), 0.02, 0.98);
        xs[static_cast<std::size_t> (i)] = std::pow (u, clustering) * spanPixels;
    }
    std::sort (xs.begin(), xs.end());

    // --- Nodos ---
    std::vector<Node> nodes;
    std::vector<int> degrees;  // grado de la escala de cada nodo, para encadenar melodías

    for (int i = 0; i < count; ++i)
    {
        Node n;
        n.id = static_cast<NodeId> (i + 1);
        n.x = std::round (xs[static_cast<std::size_t> (i)]);
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

    // El último nodo cierra la frase: tónica y algo más largo.
    nodes.back().pitches = { pitchOf (scale, root, 0) };
    nodes.back().durationBeats = std::max (nodes.back().durationBeats, 1.5);

    track.nodes = std::move (nodes);
    track.edges = std::move (edges);
    track.startNode = track.nodes.front().id;
    track.endNode = track.nodes.back().id;
}

} // namespace chimedes::model
