#pragma once

#include <cstdint>
#include <vector>

#include "model/Track.h"

namespace chimedes::model
{

// Replaces the track's graph with a randomly generated one. Nodes, edges, start
// and end change; the track's options (mode, algorithm, instrument…) are kept.
// The same seed always gives the same graph.
//
// Chosen at random:
//   - number of nodes ........... 5 to 14
//   - shape ..................... chain, branches, fan or web
//   - distribution .............. nodes spread evenly, bunched at the start or at the end
//   - closeness ................. vertical spread, from compact to wide
//   - overall length ............ 4, 6, 8, 12 or 16 beats from start to end
//   - notes ..................... a random scale and key, with the odd chord
//
// The resulting graph can always be played: every node hangs from an earlier one
// (further left), so all of them are reachable from the start, even in directed
// graphs, and the end is the rightmost node.
void randomizeGraph (Track& track, std::uint32_t seed);

// The others keep the graph (nodes, positions and edges) and change one thing only.
// The same seed always gives the same result.

// Notes: a new scale and key; the melody follows the edges from the start (each
// node takes a scale step from the one before it), with the odd chord, and the
// end resolves to the tonic.
void randomizeNotes (Track& track, std::uint32_t seed);

// Rhythm: note durations and velocities. The end node keeps its duration, so in
// timeline mode the cycle stays the same length.
void randomizeRhythm (Track& track, std::uint32_t seed);

// Instruments: the track's, chosen from `available`, and sometimes a few nodes
// with a different one. If `available` is empty, nothing changes.
void randomizeInstruments (Track& track, std::uint32_t seed, const std::vector<InstrumentId>& available);

// Algorithm: tree (Prim only on undirected graphs), traversal, siblings and beat
// snapping. The mode (timeline or algorithm) does not change.
void randomizeAlgorithm (Track& track, std::uint32_t seed);

// All of the above: a new graph with its notes, rhythm, instruments and algorithm.
void randomizeEverything (Track& track, std::uint32_t seed, const std::vector<InstrumentId>& available);

} // namespace chimedes::model
