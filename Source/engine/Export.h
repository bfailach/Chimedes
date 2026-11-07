#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "model/Track.h"

namespace chimedes::engine
{

// Cuántos pulsos exportar.
//   cycles = 0: hasta que todas las pistas vuelven a coincidir (el mínimo común
//               múltiplo de sus ciclos), sin pasar de `maxBeats`.
//   cycles > 0: esa cantidad de vueltas de la pista más larga.
// Solo cuentan las pistas que suenan (no silenciadas y con un grafo válido).
// Devuelve 0 si no suena ninguna.
double exportLengthBeats (const model::Project& project, int cycles, double maxBeats);

// Archivo MIDI estándar (formato 1, 480 divisiones por negra): una pista de tempo y
// una pista MIDI por cada pista que suena, con su nombre. Cada instrumento usa su
// canal; el 10 (percusión en General MIDI) se evita. Los instrumentos integrados
// llevan un cambio de programa General MIDI parecido a su sonido.
std::vector<std::uint8_t> exportMidi (const model::Project& project, double lengthBeats);

// Canal MIDI (0..15) de un instrumento en el archivo exportado.
int midiChannelFor (model::InstrumentId instrument) noexcept;

} // namespace chimedes::engine
