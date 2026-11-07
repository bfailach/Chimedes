#pragma once

#include <array>
#include <string>

#include "model/Track.h"

namespace chimedes::model
{

// Instruments: six built-in (synthesized) and up to ten samples loaded by the
// user. Each instrument's id is also its MIDI channel in the audio engine
// (1..16): built-ins 1 to 6, samples 7 to 16.
namespace instruments
{
    inline constexpr InstrumentId organ        = 1;
    inline constexpr InstrumentId electricPiano = 2;
    inline constexpr InstrumentId bell         = 3;
    inline constexpr InstrumentId pluck        = 4;
    inline constexpr InstrumentId pad          = 5;
    inline constexpr InstrumentId bass         = 6;

    inline constexpr InstrumentId first = organ;
    inline constexpr InstrumentId last = bass;
    inline constexpr int count = static_cast<int> (last);

    // Display names, in id order.
    inline constexpr std::array<const char*, count> names {
        "Organ", "Electric piano", "Bell", "Plucked string", "Pad", "Bass"
    };

    inline constexpr InstrumentId firstSample = 7;
    inline constexpr InstrumentId lastSample = 16;
    inline constexpr int maxSamples = static_cast<int> (lastSample - firstSample + 1);

    inline constexpr bool isBuiltIn (InstrumentId id) noexcept { return id >= first && id <= last; }
    inline constexpr bool isSample (InstrumentId id) noexcept  { return id >= firstSample && id <= lastSample; }
    inline constexpr bool isValid (InstrumentId id) noexcept   { return isBuiltIn (id) || isSample (id); }

    // Name of a built-in instrument (samples have their own, see instrumentDisplayName).
    inline const char* name (InstrumentId id) noexcept
    {
        return isBuiltIn (id) ? names[static_cast<std::size_t> (id - 1)] : names[0];
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
