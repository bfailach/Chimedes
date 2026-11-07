#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "model/Track.h"

namespace chimedes::engine
{

// How many beats to export.
//   cycles = 0: until every track lines up again (the least common multiple of
//               their cycles), never more than `maxBeats`.
//   cycles > 0: that many loops of the longest track.
// Only tracks that sound count (not muted and with a valid graph).
// Returns 0 if none of them sounds.
double exportLengthBeats (const model::Project& project, int cycles, double maxBeats);

// Standard MIDI file (format 1, 480 ticks per quarter note): a tempo track and
// one MIDI track per sounding track, with its name. Each instrument uses its own
// channel (see midiChannels); channel 10 (General MIDI percussion) is avoided.
// Built-in instruments get a General MIDI program change close to their sound.
std::vector<std::uint8_t> exportMidi (const model::Project& project, double lengthBeats);

// MIDI channel (0..15) of each instrument the sounding tracks use, in the order
// they first appear. There are 15 usable channels: from the 16th instrument on,
// channels are shared again from the first one.
std::map<model::InstrumentId, int> midiChannels (const model::Project& project);

} // namespace chimedes::engine
