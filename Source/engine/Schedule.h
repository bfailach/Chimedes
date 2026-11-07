#pragma once

#include <vector>

#include "model/Track.h"

namespace chimedes::engine
{

// A note (or chord) that has to sound within a cycle.
struct NoteEvent
{
    double startBeat = 0.0;         // relative to the start of the cycle
    double durationBeats = 1.0;
    std::vector<int> pitches;
    int velocity = 100;
    model::NodeId node = model::noNode;  // to light up the node that is sounding
    model::InstrumentId instrument = 1;  // already resolved: the node's or the track's
    model::NodeId from = model::noNode;  // node we arrived from (to animate the edge)
};

// Immutable schedule of a track: the only thing the audio thread reads.
struct Schedule
{
    std::vector<NoteEvent> events;  // sorted by startBeat
    double cycleLengthBeats = 0.0;
};

} // namespace chimedes::engine
