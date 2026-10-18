#pragma once

#include <array>

#include "model/Track.h"

namespace chimedes::model
{

// Instrumentos integrados (sintetizados). Las muestras grabadas llegan en la fase 3.
// El id de cada instrumento es también su canal MIDI en el motor de audio (1..16).
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

    inline constexpr bool isValid (InstrumentId id) noexcept { return id >= first && id <= last; }

    inline const char* name (InstrumentId id) noexcept
    {
        return isValid (id) ? names[static_cast<std::size_t> (id - 1)] : names[0];
    }
}

// El instrumento que suena para un nodo: el suyo propio o, si no tiene, el de la pista.
inline InstrumentId resolveInstrument (const Track& track, const Node& node) noexcept
{
    if (instruments::isValid (node.instrument))
        return node.instrument;

    return instruments::isValid (track.instrument) ? track.instrument : instruments::organ;
}

} // namespace chimedes::model
