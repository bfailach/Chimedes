#pragma once

#include <cstdint>

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

} // namespace chimedes::model
