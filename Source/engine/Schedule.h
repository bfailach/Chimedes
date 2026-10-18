#pragma once

#include <vector>

#include "model/Track.h"

namespace chimedes::engine
{

// Una nota (o acorde) que debe sonar dentro de un ciclo.
struct NoteEvent
{
    double startBeat = 0.0;         // relativo al inicio del ciclo
    double durationBeats = 1.0;
    std::vector<int> pitches;
    int velocity = 100;
    model::NodeId node = model::noNode;  // para iluminar el nodo que suena
    model::InstrumentId instrument = 1;  // ya resuelto: el del nodo o el de la pista
};

// Horario inmutable de una pista: lo único que lee el hilo de audio.
struct Schedule
{
    std::vector<NoteEvent> events;  // ordenados por startBeat
    double cycleLengthBeats = 0.0;
};

} // namespace chimedes::engine
