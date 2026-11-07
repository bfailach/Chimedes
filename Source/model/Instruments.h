#pragma once

#include <array>
#include <string>

#include "model/Track.h"

namespace chimedes::model
{

// Instrumentos: seis integrados (sintetizados) y hasta diez muestras cargadas por el
// usuario. El id de cada instrumento es también su canal MIDI en el motor de audio
// (1..16): integrados del 1 al 6, muestras del 7 al 16.
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

    // Nombres en UTF-8, en el orden de los ids.
    inline constexpr std::array<const char*, count> names {
        "Órgano", "Piano eléctrico", "Campana", "Cuerda pulsada", "Pad", "Bajo"
    };

    inline constexpr InstrumentId firstSample = 7;
    inline constexpr InstrumentId lastSample = 16;
    inline constexpr int maxSamples = static_cast<int> (lastSample - firstSample + 1);

    inline constexpr bool isBuiltIn (InstrumentId id) noexcept { return id >= first && id <= last; }
    inline constexpr bool isSample (InstrumentId id) noexcept  { return id >= firstSample && id <= lastSample; }
    inline constexpr bool isValid (InstrumentId id) noexcept   { return isBuiltIn (id) || isSample (id); }

    // Nombre de un instrumento integrado (las muestras tienen el suyo, ver displayName).
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

// Nombre para mostrar: el del instrumento integrado o el de la muestra.
inline std::string instrumentDisplayName (const Project& project, InstrumentId id)
{
    if (instruments::isBuiltIn (id))
        return instruments::name (id);
    if (const auto* s = findSample (project, id))
        return s->name;
    return instruments::name (instruments::organ);
}

// Primer id de muestra libre, o 0 si ya hay diez.
inline InstrumentId freeSampleId (const Project& project) noexcept
{
    for (auto id = instruments::firstSample; id <= instruments::lastSample; ++id)
        if (findSample (project, id) == nullptr)
            return id;
    return 0;
}

// El instrumento que suena para un nodo: el suyo propio o, si no tiene, el de la pista.
inline InstrumentId resolveInstrument (const Track& track, const Node& node) noexcept
{
    if (instruments::isValid (node.instrument))
        return node.instrument;

    return instruments::isValid (track.instrument) ? track.instrument : instruments::organ;
}

} // namespace chimedes::model
