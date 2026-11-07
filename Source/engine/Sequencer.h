#pragma once

#include <array>
#include <cmath>

#include "engine/Schedule.h"

namespace chimedes::engine
{

// Turns a track's schedule into note-on / note-off messages, block by block.
//
// Safe for the audio thread: it neither allocates nor locks. It works in absolute
// beats since playback started and repeats the schedule in cycles.
//
// Note-offs come from its own list of active notes, not from the schedule. So if the
// schedule changes in the middle of a note (live editing), that note ends normally.
class Sequencer
{
public:
    static constexpr int maxActiveNotes = 256;

    // The schedule is owned elsewhere; it must stay alive while it is set here.
    // When it changes, the cursor moves to the current position in the cycle.
    void setSchedule (const Schedule* newSchedule) noexcept
    {
        if (newSchedule != schedule)
        {
            schedule = newSchedule;
            needsResync = true;
        }
    }

    const Schedule* getSchedule() const noexcept { return schedule; }

    // Goes back to the beginning (on stop). Call allNotesOff first if notes are active.
    void rewind() noexcept
    {
        needsResync = true;
        numActive = 0;
    }

    // Emits the messages for [fromBeat, toBeat). The sink is called as
    // sink (double beat, int pitch, int velocity, int instrument, bool isNoteOn).
    template <typename Sink>
    void process (double fromBeat, double toBeat, Sink&& sink)
    {
        emitNoteOns (fromBeat, toBeat, sink);
        emitNoteOffs (toBeat, sink);
    }

    // Turns off every active note at the given beat (on pause or stop).
    template <typename Sink>
    void allNotesOff (double beat, Sink&& sink)
    {
        for (int i = 0; i < numActive; ++i)
        {
            const auto& note = active[static_cast<std::size_t> (i)];
            sink (beat, note.pitch, 0, note.instrument, false);
        }

        numActive = 0;
    }

    int getNumActiveNotes() const noexcept { return numActive; }

private:
    struct ActiveNote
    {
        int pitch = 0;
        int instrument = 1;
        double endBeat = 0.0;
    };

    // Moves a cursor through the schedule's events. Each event is emitted exactly once
    // per cycle, without depending on how beats round between blocks.
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
                startNote (beat, pitch, event.velocity, static_cast<int> (event.instrument),
                           beat + event.durationBeats, sink);

            ++nextEvent;
        }
    }

    // Places the cursor on the first event that has not sounded yet at `beat`.
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
                sink (note.endBeat, note.pitch, 0, note.instrument, false);
                removeAt (i);
            }
            else
            {
                ++i;
            }
        }
    }

    template <typename Sink>
    void startNote (double beat, int pitch, int velocity, int instrument, double endBeat, Sink& sink)
    {
        // If the same note is already sounding on the same instrument, it is turned off
        // before playing it again, so its pending note-off does not cut the new one.
        for (int i = 0; i < numActive; ++i)
        {
            const auto& note = active[static_cast<std::size_t> (i)];
            if (note.pitch == pitch && note.instrument == instrument)
            {
                sink (beat, pitch, 0, instrument, false);
                removeAt (i);
                break;
            }
        }

        // No room left: the note that was going to end soonest is turned off.
        if (numActive == maxActiveNotes)
        {
            int earliest = 0;
            for (int i = 1; i < numActive; ++i)
                if (active[static_cast<std::size_t> (i)].endBeat < active[static_cast<std::size_t> (earliest)].endBeat)
                    earliest = i;

            const auto& oldest = active[static_cast<std::size_t> (earliest)];
            sink (beat, oldest.pitch, 0, oldest.instrument, false);
            removeAt (earliest);
        }

        sink (beat, pitch, velocity, instrument, true);
        active[static_cast<std::size_t> (numActive++)] = { pitch, instrument, endBeat };
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
