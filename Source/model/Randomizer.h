#pragma once

#include <cstdint>
#include <vector>

#include "model/Track.h"

namespace chimedes::model
{

// Sustituye el grafo de la pista por uno generado al azar. Cambian los nodos, las
// aristas, el inicio y el fin; las opciones de la pista (modo, algoritmo, instrumento…)
// se conservan. Con la misma semilla se obtiene siempre el mismo grafo.
//
// Se eligen al azar:
//   - cantidad de nodos ......... de 5 a 14
//   - forma ..................... cadena, ramas, abanico o red
//   - distribución .............. nodos repartidos parejo, apiñados al principio o al final
//   - cercanía .................. dispersión vertical, de compacta a amplia
//   - duración general .......... 4, 6, 8, 12 o 16 pulsos de inicio a fin
//   - notas ..................... una escala y tonalidad al azar, con algún acorde
//
// El grafo resultante siempre se puede reproducir: cada nodo cuelga de uno anterior
// (más a la izquierda), así que todos son alcanzables desde el inicio, incluso en
// grafos dirigidos, y el fin es el nodo más a la derecha.
void randomizeGraph (Track& track, std::uint32_t seed);

// Las demás conservan el grafo (nodos, posiciones y aristas) y cambian una sola cosa.
// Con la misma semilla dan siempre el mismo resultado.

// Notas: una escala y tonalidad nuevas; la melodía sigue las aristas desde el inicio
// (cada nodo da un paso por la escala desde el que lo precede), con algún acorde, y el
// fin cierra en la tónica.
void randomizeNotes (Track& track, std::uint32_t seed);

// Ritmo: duraciones e intensidades de las notas. El nodo de fin conserva su duración,
// para que en línea de tiempo el ciclo siga durando lo mismo.
void randomizeRhythm (Track& track, std::uint32_t seed);

// Instrumentos: el de la pista, elegido entre `available`, y a veces algunos nodos con
// otro distinto. Si `available` está vacío, no cambia nada.
void randomizeInstruments (Track& track, std::uint32_t seed, const std::vector<InstrumentId>& available);

// Algoritmo: árbol (Prim solo en grafos no dirigidos), recorrido, hermanos y ajuste al
// pulso. El modo (línea de tiempo o algoritmo) no cambia.
void randomizeAlgorithm (Track& track, std::uint32_t seed);

// Todo lo anterior: grafo nuevo con sus notas, ritmo, instrumentos y algoritmo.
void randomizeEverything (Track& track, std::uint32_t seed, const std::vector<InstrumentId>& available);

} // namespace chimedes::model
