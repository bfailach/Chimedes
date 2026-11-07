#include "model/ProjectFile.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <variant>
#include <vector>

namespace chimedes::model
{

namespace
{
    // -----------------------------------------------------------------------
    // JSON mínimo: lo justo para este formato, sin depender de JUCE (así el
    // motor y sus pruebas siguen compilando solos). Los números se leen y
    // escriben con <charconv>, que no depende del idioma del sistema (una coma
    // decimal en Windows en español no rompe el archivo).
    // -----------------------------------------------------------------------

    struct Json;
    using JsonArray = std::vector<Json>;
    using JsonObject = std::map<std::string, Json>;

    struct Json
    {
        std::variant<std::nullptr_t, bool, double, std::string,
                     std::shared_ptr<JsonArray>, std::shared_ptr<JsonObject>> value = nullptr;

        const JsonObject* object() const
        {
            const auto* p = std::get_if<std::shared_ptr<JsonObject>> (&value);
            return p != nullptr ? p->get() : nullptr;
        }

        const JsonArray* array() const
        {
            const auto* p = std::get_if<std::shared_ptr<JsonArray>> (&value);
            return p != nullptr ? p->get() : nullptr;
        }

        std::optional<double> number() const
        {
            if (const auto* d = std::get_if<double> (&value))
                return *d;
            return std::nullopt;
        }

        std::optional<bool> boolean() const
        {
            if (const auto* b = std::get_if<bool> (&value))
                return *b;
            return std::nullopt;
        }

        const std::string* string() const { return std::get_if<std::string> (&value); }
    };

    class Parser
    {
    public:
        explicit Parser (const std::string& textToParse) : text (textToParse) {}

        Json parseDocument()
        {
            auto result = parseValue (0);
            skipSpace();
            if (pos != text.size())
                fail ("hay texto de más al final");
            return result;
        }

    private:
        [[noreturn]] void fail (const std::string& what) const
        {
            // Línea donde está el error, para quien edite el archivo a mano.
            const auto line = 1 + std::count (text.begin(), text.begin() + static_cast<std::ptrdiff_t> (std::min (pos, text.size())), '\n');
            throw std::runtime_error (what + " (línea " + std::to_string (line) + ")");
        }

        void skipSpace()
        {
            while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' || text[pos] == '\r'))
                ++pos;
        }

        bool consume (char c)
        {
            skipSpace();
            if (pos < text.size() && text[pos] == c)
            {
                ++pos;
                return true;
            }
            return false;
        }

        void expect (char c)
        {
            if (! consume (c))
                fail (std::string ("se esperaba '") + c + "'");
        }

        Json parseValue (int depth)
        {
            if (depth > 64)
                fail ("anidamiento demasiado profundo");

            skipSpace();
            if (pos >= text.size())
                fail ("el archivo termina antes de tiempo");

            const auto c = text[pos];

            if (c == '{')  return parseObject (depth);
            if (c == '[')  return parseArray (depth);
            if (c == '"')  return { parseString() };
            if (c == 't' && text.compare (pos, 4, "true") == 0)   { pos += 4; return { true }; }
            if (c == 'f' && text.compare (pos, 5, "false") == 0)  { pos += 5; return { false }; }
            if (c == 'n' && text.compare (pos, 4, "null") == 0)   { pos += 4; return {}; }
            if (c == '-' || (c >= '0' && c <= '9'))               return { parseNumber() };

            fail ("valor no válido");
        }

        Json parseObject (int depth)
        {
            expect ('{');
            auto object = std::make_shared<JsonObject>();

            if (! consume ('}'))
            {
                do
                {
                    skipSpace();
                    if (pos >= text.size() || text[pos] != '"')
                        fail ("se esperaba el nombre de un campo entre comillas");
                    auto key = parseString();
                    expect (':');
                    (*object)[key] = parseValue (depth + 1);
                }
                while (consume (','));

                expect ('}');
            }

            return { object };
        }

        Json parseArray (int depth)
        {
            expect ('[');
            auto array = std::make_shared<JsonArray>();

            if (! consume (']'))
            {
                do
                    array->push_back (parseValue (depth + 1));
                while (consume (','));

                expect (']');
            }

            return { array };
        }

        double parseNumber()
        {
            const auto* begin = text.data() + pos;
            const auto* end = text.data() + text.size();
            double value = 0.0;
            const auto [next, error] = std::from_chars (begin, end, value);
            if (error != std::errc() || ! std::isfinite (value))
                fail ("número no válido");
            pos += static_cast<std::size_t> (next - begin);
            return value;
        }

        std::string parseString()
        {
            ++pos;  // comilla inicial
            std::string out;

            while (pos < text.size())
            {
                const auto c = text[pos++];

                if (c == '"')
                    return out;

                if (c != '\\')
                {
                    out += c;   // UTF-8 se copia tal cual
                    continue;
                }

                if (pos >= text.size())
                    break;

                const auto e = text[pos++];
                switch (e)
                {
                    case '"': case '\\': case '/': out += e; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u':
                    {
                        if (pos + 4 > text.size())
                            fail ("escape \\u incompleto");
                        unsigned code = 0;
                        const auto [p, err] = std::from_chars (text.data() + pos, text.data() + pos + 4, code, 16);
                        if (err != std::errc() || p != text.data() + pos + 4)
                            fail ("escape \\u no válido");
                        pos += 4;
                        appendUtf8 (out, code);
                        break;
                    }
                    default: fail ("escape no válido en un texto");
                }
            }

            fail ("falta la comilla de cierre de un texto");
        }

        static void appendUtf8 (std::string& out, unsigned code)
        {
            if (code < 0x80)
            {
                out += static_cast<char> (code);
            }
            else if (code < 0x800)
            {
                out += static_cast<char> (0xC0 | (code >> 6));
                out += static_cast<char> (0x80 | (code & 0x3F));
            }
            else
            {
                out += static_cast<char> (0xE0 | (code >> 12));
                out += static_cast<char> (0x80 | ((code >> 6) & 0x3F));
                out += static_cast<char> (0x80 | (code & 0x3F));
            }
        }

        const std::string& text;
        std::size_t pos = 0;
    };

    // --- Escritura ----------------------------------------------------------

    std::string number (double value)
    {
        if (! std::isfinite (value))
            value = 0.0;

        char buffer[64];
        const auto [end, error] = std::to_chars (buffer, buffer + sizeof (buffer), value);
        return error == std::errc() ? std::string (buffer, end) : "0";
    }

    std::string quoted (const std::string& s)
    {
        std::string out = "\"";
        for (const auto c : s)
        {
            switch (c)
            {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char> (c) < 0x20)
                    {
                        char hex[8];
                        std::snprintf (hex, sizeof (hex), "\\u%04x", static_cast<unsigned> (static_cast<unsigned char> (c)));
                        out += hex;
                    }
                    else
                    {
                        out += c;
                    }
            }
        }
        return out + "\"";
    }

    // Opciones como palabras: el archivo se entiende al leerlo.
    template <typename Enum, std::size_t N>
    struct Names
    {
        std::array<std::pair<Enum, const char*>, N> items;

        const char* name (Enum value) const
        {
            for (const auto& [v, n] : items)
                if (v == value)
                    return n;
            return items[0].second;
        }

        Enum parse (const std::string* text, Enum fallback) const
        {
            if (text != nullptr)
                for (const auto& [v, n] : items)
                    if (*text == n)
                        return v;
            return fallback;
        }
    };

    const Names<PlaybackMode, 2> modeNames { { { { PlaybackMode::Timeline, "timeline" }, { PlaybackMode::Algorithm, "algorithm" } } } };
    const Names<TreeMethod, 4> treeNames { { { { TreeMethod::BFS, "bfs" }, { TreeMethod::DFS, "dfs" },
                                               { TreeMethod::Dijkstra, "dijkstra" }, { TreeMethod::Prim, "prim" } } } };
    const Names<Traversal, 2> traversalNames { { { { Traversal::BFS, "bfs" }, { Traversal::DFS, "dfs" } } } };
    const Names<SiblingMode, 2> siblingNames { { { { SiblingMode::Sequential, "sequential" }, { SiblingMode::Simultaneous, "simultaneous" } } } };

    // --- Lectura con valores por defecto -------------------------------------

    const Json* field (const JsonObject& object, const char* key)
    {
        const auto it = object.find (key);
        return it != object.end() ? &it->second : nullptr;
    }

    double numberOr (const JsonObject& object, const char* key, double fallback)
    {
        const auto* f = field (object, key);
        return f != nullptr && f->number() ? *f->number() : fallback;
    }

    bool boolOr (const JsonObject& object, const char* key, bool fallback)
    {
        const auto* f = field (object, key);
        return f != nullptr && f->boolean() ? *f->boolean() : fallback;
    }

    std::uint32_t idOr (const JsonObject& object, const char* key, std::uint32_t fallback)
    {
        const auto v = numberOr (object, key, -1.0);
        return v >= 0.0 && v <= 4.0e9 && v == std::floor (v) ? static_cast<std::uint32_t> (v) : fallback;
    }

    const std::string* stringOf (const JsonObject& object, const char* key)
    {
        const auto* f = field (object, key);
        return f != nullptr ? f->string() : nullptr;
    }

    Node readNode (const JsonObject& o)
    {
        Node n;
        n.id = idOr (o, "id", noNode);
        n.x = numberOr (o, "x", 0.0);
        n.y = numberOr (o, "y", 0.0);
        n.durationBeats = std::clamp (numberOr (o, "duration", 1.0), 0.05, 64.0);
        n.velocity = static_cast<int> (std::clamp (std::round (numberOr (o, "velocity", 100.0)), 1.0, 127.0));
        n.instrument = idOr (o, "instrument", 0);

        if (const auto* f = field (o, "pitches"); f != nullptr && f->array() != nullptr)
            for (const auto& p : *f->array())
                if (const auto v = p.number())
                    n.pitches.push_back (static_cast<int> (std::clamp (std::round (*v), 0.0, 127.0)));

        std::sort (n.pitches.begin(), n.pitches.end());
        n.pitches.erase (std::unique (n.pitches.begin(), n.pitches.end()), n.pitches.end());
        if (n.pitches.empty())
            n.pitches = { 60 };

        return n;
    }

    Track readTrack (const JsonObject& o, TrackId fallbackId)
    {
        Track t;
        t.id = idOr (o, "id", fallbackId);
        if (const auto* name = stringOf (o, "name"))
            t.name = *name;
        t.directed = boolOr (o, "directed", true);
        t.mode = modeNames.parse (stringOf (o, "mode"), PlaybackMode::Timeline);
        t.treeMethod = treeNames.parse (stringOf (o, "treeMethod"), TreeMethod::BFS);
        t.traversal = traversalNames.parse (stringOf (o, "traversal"), Traversal::BFS);
        t.siblings = siblingNames.parse (stringOf (o, "siblings"), SiblingMode::Sequential);
        t.quantizeBeats = std::clamp (numberOr (o, "quantize", 0.0), 0.0, 16.0);
        t.instrument = idOr (o, "instrument", 1);
        t.muted = boolOr (o, "muted", false);

        // Nodos: sin id válido o con id repetido se les da uno nuevo.
        std::set<NodeId> used;
        if (const auto* f = field (o, "nodes"); f != nullptr && f->array() != nullptr)
            for (const auto& item : *f->array())
                if (const auto* obj = item.object())
                {
                    auto n = readNode (*obj);
                    if (n.id == noNode || used.count (n.id) > 0)
                        n.id = noNode;
                    else
                        used.insert (n.id);
                    t.nodes.push_back (std::move (n));
                }

        NodeId nextId = used.empty() ? 1 : *used.rbegin() + 1;
        for (auto& n : t.nodes)
            if (n.id == noNode)
                n.id = nextId++;

        const auto exists = [&t] (NodeId id) { return t.findNode (id) != nullptr; };

        // Aristas: solo entre nodos que existen, sin bucles ni repetidas.
        std::set<std::pair<NodeId, NodeId>> seen;
        EdgeId nextEdge = 1;
        if (const auto* f = field (o, "edges"); f != nullptr && f->array() != nullptr)
            for (const auto& item : *f->array())
                if (const auto* obj = item.object())
                {
                    Edge e;
                    e.from = idOr (*obj, "from", noNode);
                    e.to = idOr (*obj, "to", noNode);
                    if (! exists (e.from) || ! exists (e.to) || e.from == e.to)
                        continue;

                    const auto key = t.directed ? std::make_pair (e.from, e.to)
                                                : std::make_pair (std::min (e.from, e.to), std::max (e.from, e.to));
                    if (! seen.insert (key).second)
                        continue;

                    e.id = nextEdge++;
                    t.edges.push_back (e);
                }

        const auto start = idOr (o, "start", noNode);
        const auto end = idOr (o, "end", noNode);
        t.startNode = exists (start) ? start : noNode;
        t.endNode = exists (end) ? end : noNode;

        return t;
    }
}

std::string projectToJson (const Project& project)
{
    std::string out;
    out += "{\n";
    out += "  \"format\": \"chimedes\",\n";
    out += "  \"version\": " + std::to_string (currentProjectFormatVersion) + ",\n";
    out += "  \"bpm\": " + number (project.bpm) + ",\n";
    out += "  \"tracks\": [";

    for (std::size_t ti = 0; ti < project.tracks.size(); ++ti)
    {
        const auto& t = project.tracks[ti];
        out += ti == 0 ? "\n" : ",\n";
        out += "    {\n";
        out += "      \"id\": " + std::to_string (t.id) + ",\n";
        out += "      \"name\": " + quoted (t.name) + ",\n";
        out += "      \"directed\": " + std::string (t.directed ? "true" : "false") + ",\n";
        out += "      \"mode\": " + quoted (modeNames.name (t.mode)) + ",\n";
        out += "      \"treeMethod\": " + quoted (treeNames.name (t.treeMethod)) + ",\n";
        out += "      \"traversal\": " + quoted (traversalNames.name (t.traversal)) + ",\n";
        out += "      \"siblings\": " + quoted (siblingNames.name (t.siblings)) + ",\n";
        out += "      \"quantize\": " + number (t.quantizeBeats) + ",\n";
        out += "      \"instrument\": " + std::to_string (t.instrument) + ",\n";
        out += "      \"muted\": " + std::string (t.muted ? "true" : "false") + ",\n";
        out += "      \"start\": " + std::to_string (t.startNode) + ",\n";
        out += "      \"end\": " + std::to_string (t.endNode) + ",\n";

        // Un nodo y una arista por línea: el archivo sigue siendo fácil de leer y de comparar.
        out += "      \"nodes\": [";
        for (std::size_t i = 0; i < t.nodes.size(); ++i)
        {
            const auto& n = t.nodes[i];
            std::string pitches;
            for (std::size_t p = 0; p < n.pitches.size(); ++p)
                pitches += (p == 0 ? "" : ", ") + std::to_string (n.pitches[p]);

            out += i == 0 ? "\n" : ",\n";
            out += "        { \"id\": " + std::to_string (n.id)
                 + ", \"x\": " + number (n.x)
                 + ", \"y\": " + number (n.y)
                 + ", \"pitches\": [" + pitches + "]"
                 + ", \"duration\": " + number (n.durationBeats)
                 + ", \"velocity\": " + std::to_string (n.velocity)
                 + ", \"instrument\": " + std::to_string (n.instrument) + " }";
        }
        out += t.nodes.empty() ? "],\n" : "\n      ],\n";

        out += "      \"edges\": [";
        for (std::size_t i = 0; i < t.edges.size(); ++i)
        {
            const auto& e = t.edges[i];
            out += i == 0 ? "\n" : ",\n";
            out += "        { \"id\": " + std::to_string (e.id)
                 + ", \"from\": " + std::to_string (e.from)
                 + ", \"to\": " + std::to_string (e.to) + " }";
        }
        out += t.edges.empty() ? "]\n" : "\n      ]\n";
        out += "    }";
    }

    out += project.tracks.empty() ? "]\n" : "\n  ]\n";
    out += "}\n";
    return out;
}

ProjectLoadResult projectFromJson (const std::string& json)
{
    ProjectLoadResult result;

    Json root;
    try
    {
        root = Parser (json).parseDocument();
    }
    catch (const std::exception& e)
    {
        result.error = std::string ("El archivo está dañado: ") + e.what() + ".";
        return result;
    }

    const auto* object = root.object();
    const auto* format = object != nullptr ? stringOf (*object, "format") : nullptr;
    if (format == nullptr || *format != "chimedes")
    {
        result.error = "No es un proyecto de Chimedes.";
        return result;
    }

    const auto version = numberOr (*object, "version", 1.0);
    if (version > currentProjectFormatVersion)
    {
        result.error = "El proyecto se creó con una versión más nueva de Chimedes.";
        return result;
    }

    result.project.formatVersion = currentProjectFormatVersion;
    result.project.bpm = std::clamp (numberOr (*object, "bpm", 120.0), 20.0, 400.0);

    if (const auto* f = field (*object, "tracks"); f != nullptr && f->array() != nullptr)
    {
        TrackId nextTrack = 1;
        for (const auto& item : *f->array())
            if (const auto* obj = item.object())
                result.project.tracks.push_back (readTrack (*obj, nextTrack++));
    }

    if (result.project.tracks.empty())
    {
        Track empty;
        empty.id = 1;
        result.project.tracks.push_back (std::move (empty));
    }

    result.ok = true;
    return result;
}

} // namespace chimedes::model
