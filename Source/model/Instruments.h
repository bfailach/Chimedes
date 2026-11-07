#pragma once

#include <array>
#include <string>
#include <vector>

#include "model/Track.h"

namespace chimedes::model
{

// Instruments: built-in (synthesized) ones and up to ten samples loaded by the
// user. Ids are stored in project files, so they never change:
//   - built-ins 1..6 (the original set) and 17..32 (added later);
//   - samples 7..16 (each one is also its MIDI channel in the audio engine).
namespace instruments
{
    enum class Category { Keys, Mallets, Strings, Winds, Synth, Bass };

    inline constexpr std::array<const char*, 6> categoryNames {
        "Keys", "Mallets", "Strings", "Winds", "Synth", "Bass"
    };

    inline constexpr InstrumentId organ         = 1;
    inline constexpr InstrumentId electricPiano = 2;
    inline constexpr InstrumentId bell          = 3;
    inline constexpr InstrumentId pluck         = 4;
    inline constexpr InstrumentId pad           = 5;
    inline constexpr InstrumentId bass          = 6;

    inline constexpr InstrumentId piano         = 17;
    inline constexpr InstrumentId harpsichord   = 18;
    inline constexpr InstrumentId musicBox      = 19;
    inline constexpr InstrumentId marimba       = 20;
    inline constexpr InstrumentId vibraphone    = 21;
    inline constexpr InstrumentId kalimba       = 22;
    inline constexpr InstrumentId steelDrum     = 23;
    inline constexpr InstrumentId harp          = 24;
    inline constexpr InstrumentId strings       = 25;
    inline constexpr InstrumentId flute         = 26;
    inline constexpr InstrumentId brass         = 27;
    inline constexpr InstrumentId choir         = 28;
    inline constexpr InstrumentId lead          = 29;
    inline constexpr InstrumentId synthPluck    = 30;
    inline constexpr InstrumentId subBass       = 31;
    inline constexpr InstrumentId acidBass      = 32;

    struct Info
    {
        InstrumentId id;
        const char* name;
        Category category;
        int gmProgram;      // closest General MIDI program (0-based), for MIDI export
    };

    // Every built-in, in menu order (grouped by category).
    inline constexpr std::array<Info, 22> builtIns { {
        { piano,         "Piano",          Category::Keys,     0 },
        { electricPiano, "Electric piano", Category::Keys,     4 },
        { organ,         "Organ",          Category::Keys,    16 },
        { harpsichord,   "Harpsichord",    Category::Keys,     6 },
        { musicBox,      "Music box",      Category::Keys,    10 },

        { bell,          "Bell",           Category::Mallets, 14 },
        { marimba,       "Marimba",        Category::Mallets, 12 },
        { vibraphone,    "Vibraphone",     Category::Mallets, 11 },
        { kalimba,       "Kalimba",        Category::Mallets, 108 },
        { steelDrum,     "Steel drum",     Category::Mallets, 114 },

        { pluck,         "Plucked string", Category::Strings, 24 },
        { harp,          "Harp",           Category::Strings, 46 },
        { strings,       "Strings",        Category::Strings, 48 },

        { flute,         "Flute",          Category::Winds,   73 },
        { brass,         "Brass",          Category::Winds,   61 },
        { choir,         "Choir",          Category::Winds,   52 },

        { pad,           "Pad",            Category::Synth,   89 },
        { lead,          "Lead",           Category::Synth,   81 },
        { synthPluck,    "Synth pluck",    Category::Synth,   84 },

        { bass,          "Bass",           Category::Bass,    33 },
        { subBass,       "Sub bass",       Category::Bass,    38 },
        { acidBass,      "Acid bass",      Category::Bass,    87 },
    } };

    inline constexpr int count = static_cast<int> (builtIns.size());

    inline constexpr InstrumentId firstSample = 7;
    inline constexpr InstrumentId lastSample = 16;
    inline constexpr int maxSamples = static_cast<int> (lastSample - firstSample + 1);

    // Largest id a built-in can have (the audio engine sizes its tables with it).
    inline constexpr InstrumentId maxId = 32;

    inline constexpr const Info* info (InstrumentId id) noexcept
    {
        for (const auto& i : builtIns)
            if (i.id == id)
                return &i;
        return nullptr;
    }

    inline constexpr bool isBuiltIn (InstrumentId id) noexcept { return info (id) != nullptr; }
    inline constexpr bool isSample (InstrumentId id) noexcept  { return id >= firstSample && id <= lastSample; }
    inline constexpr bool isValid (InstrumentId id) noexcept   { return isBuiltIn (id) || isSample (id); }

    // Name of a built-in instrument (samples have their own, see instrumentDisplayName).
    inline constexpr const char* name (InstrumentId id) noexcept
    {
        const auto* i = info (id);
        return i != nullptr ? i->name : "Organ";
    }

    // General MIDI program of a built-in, or -1 for samples.
    inline constexpr int gmProgram (InstrumentId id) noexcept
    {
        const auto* i = info (id);
        return i != nullptr ? i->gmProgram : -1;
    }

    // The ids of every built-in, in menu order.
    inline std::vector<InstrumentId> builtInIds()
    {
        std::vector<InstrumentId> ids;
        for (const auto& i : builtIns)
            ids.push_back (i.id);
        return ids;
    }
}

inline const Sample* findSample (const Project& project, InstrumentId id) noexcept
{
    for (const auto& s : project.samples)
        if (s.id == id)
            return &s;
    return nullptr;
}

// Display name: the built-in instrument's or the sample's.
inline std::string instrumentDisplayName (const Project& project, InstrumentId id)
{
    if (instruments::isBuiltIn (id))
        return instruments::name (id);
    if (const auto* s = findSample (project, id))
        return s->name;
    return instruments::name (instruments::organ);
}

// First free sample id, or 0 if there are already ten.
inline InstrumentId freeSampleId (const Project& project) noexcept
{
    for (auto id = instruments::firstSample; id <= instruments::lastSample; ++id)
        if (findSample (project, id) == nullptr)
            return id;
    return 0;
}

// The instrument a node plays with: its own or, if it has none, the track's.
inline InstrumentId resolveInstrument (const Track& track, const Node& node) noexcept
{
    if (instruments::isValid (node.instrument))
        return node.instrument;

    return instruments::isValid (track.instrument) ? track.instrument : instruments::organ;
}

} // namespace chimedes::model
