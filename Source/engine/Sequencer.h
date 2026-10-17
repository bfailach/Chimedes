#pragma once

#include <array>
#include <cmath>

#include "engine/Schedule.h"

namespace chimedes::engine
{

// Convierte el horario de una pista en mensajes note-on / note-off, bloque a bloque.
//
// Es seguro para el hilo de audio: no asigna memoria ni bloquea. Trabaja en pulsos
// absolutos desde que empezó la reproducción y repite el horario en ciclos.
//
// Los note-off salen de su propia lista de notas activas, no del horario. Así, si el
// horario cambia a mitad de una nota (edición en vivo), esa nota termina con normalidad.
class Sequencer
{
public:
    static constexpr int maxActiveNotes = 256;

    // El horario lo posee otra parte; debe seguir vivo mientras esté puesto aquí.
    // Al cambiarlo, el cursor se recoloca en la posición actual del ciclo.
    void setSchedule (const Schedule* newSchedule) noexcept
    {
        if (newSchedule != schedule)
        {
            schedule = newSchedule;
            needsResync = true;
        }
    }

    const Schedule* getSchedule() const noexcept { return schedule; }

    // Vuelve al principio (al detener). Llama antes a allNotesOff si hay notas activas.
    void rewind() noexcept
    {
        needsResync = true;
        numActive = 0;
    }

    // Emite los mensajes del tramo [fromBeat, toBeat). El receptor se llama como
    // sink (double beat, int pitch, int velocity, bool isNoteOn).
    template <typename Sink>
    void process (double fromBeat, double toBeat, Sink&& sink)
    {
        emitNoteOns (fromBeat, toBeat, sink);
        emitNoteOffs (toBeat, sink);
    }

    // Apaga todas las notas activas en el pulso indicado (al pausar o detener).
    template <typename Sink>
    void allNotesOff (double beat, Sink&& sink)
    {
        for (int i = 0; i < numActive; ++i)
            sink (beat, active[static_cast<std::size_t> (i)].pitch, 0, false);

        numActive = 0;
    }

    int getNumActiveNotes() const noexcept { return numActive; }

private:
    struct ActiveNote
    {
        int pitch = 0;
        double endBeat = 0.0;
    };

    // Avanza un cursor por los eventos del horario. Cada evento se emite una sola vez
    // por ciclo, sin depender del redondeo de los pulsos entre bloques.
    template <typename Sink>
    void emitNoteOns (double fromBeat, double toBeat, Sink& sink)
    {
        if (schedule == nullptr || schedule->events.empty() || schedule->cycleLengthBeats <= 0.0)
            return;

        if (needsResync)
            resync (fromBeat);

        const auto& events = schedule->events;
        const auto cycle = schedule->cycleLengthBeats;

        for (;;)
        {
            if (nextEvent >= events.size())
            {
                cycleStart += cycle;
                nextEvent = 0;
            }

            const auto& event = events[nextEvent];
            const auto beat = cycleStart + event.startBeat;

            if (beat >= toBeat)
                break;

            for (const auto pitch : event.pitches)
                startNote (beat, pitch, event.velocity, beat + event.durationBeats, sink);

            ++nextEvent;
        }
    }

    // Coloca el cursor en el primer evento que aún no ha sonado en `beat`.
    void resync (double beat) noexcept
    {
        const auto cycle = schedule->cycleLengthBeats;
        cycleStart = std::floor (beat / cycle) * cycle;

        const auto offset = beat - cycleStart;
        const auto& events = schedule->events;

        nextEvent = 0;
        while (nextEvent < events.size() && events[nextEvent].startBeat < offset)
            ++nextEvent;

        needsResync = false;
    }

    template <typename Sink>
    void emitNoteOffs (double toBeat, Sink& sink)
    {
        for (int i = 0; i < numActive;)
        {
            const auto note = active[static_cast<std::size_t> (i)];

            if (note.endBeat < toBeat)
            {
                sink (note.endBeat, note.pitch, 0, false);
                removeAt (i);
            }
            else
            {
                ++i;
            }
        }
    }

    template <typename Sink>
    void startNote (double beat, int pitch, int velocity, double endBeat, Sink& sink)
    {
        // Si la misma nota ya suena, se apaga antes de volver a tocarla, para que su
        // note-off pendiente no corte la nueva.
        for (int i = 0; i < numActive; ++i)
        {
            if (active[static_cast<std::size_t> (i)].pitch == pitch)
            {
                sink (beat, pitch, 0, false);
                removeAt (i);
                break;
            }
        }

        // Sin espacio: se apaga la nota que antes iba a terminar.
        if (numActive == maxActiveNotes)
        {
            int earliest = 0;
            for (int i = 1; i < numActive; ++i)
                if (active[static_cast<std::size_t> (i)].endBeat < active[static_cast<std::size_t> (earliest)].endBeat)
                    earliest = i;

            sink (beat, active[static_cast<std::size_t> (earliest)].pitch, 0, false);
            removeAt (earliest);
        }

        sink (beat, pitch, velocity, true);
        active[static_cast<std::size_t> (numActive++)] = { pitch, endBeat };
    }

    void removeAt (int index) noexcept
    {
        active[static_cast<std::size_t> (index)] = active[static_cast<std::size_t> (numActive - 1)];
        --numActive;
    }

    const Schedule* schedule = nullptr;
    std::size_t nextEvent = 0;
    double cycleStart = 0.0;
    bool needsResync = true;

    std::array<ActiveNote, maxActiveNotes> active {};
    int numActive = 0;
};

} // namespace chimedes::engine
