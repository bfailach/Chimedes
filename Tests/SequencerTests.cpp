#include "TestFramework.h"

#include "engine/Sequencer.h"

using namespace chimedes;

namespace
{
    struct Message
    {
        double beat;
        int pitch;
        bool on;
    };

    engine::NoteEvent note (double start, double duration, int pitch)
    {
        return { start, duration, { pitch }, 100, 0 };
    }

    // Reproduce de `from` a `to` en bloques de `block` pulsos y devuelve los mensajes.
    std::vector<Message> play (engine::Sequencer& seq, double from, double to, double block)
    {
        std::vector<Message> out;
        const auto sink = [&out] (double beat, int pitch, int, bool on) { out.push_back ({ beat, pitch, on }); };

        for (auto b = from; b < to; b += block)
            seq.process (b, std::min (b + block, to), sink);

        return out;
    }

    int countOn (const std::vector<Message>& msgs, int pitch)
    {
        int n = 0;
        for (const auto& m : msgs)
            n += (m.on && m.pitch == pitch) ? 1 : 0;
        return n;
    }

    // Dos notas en un ciclo de 4 pulsos: do en 0 y mi en 2, de 1 pulso cada una.
    engine::Schedule twoNotes()
    {
        engine::Schedule s;
        s.events = { note (0.0, 1.0, 60), note (2.0, 1.0, 64) };
        s.cycleLengthBeats = 4.0;
        return s;
    }
}

TEST ("secuenciador: repite el horario en ciclos con sus note-off")
{
    const auto schedule = twoNotes();
    engine::Sequencer seq;
    seq.setSchedule (&schedule);

    const auto msgs = play (seq, 0.0, 12.0, 0.37);

    // 3 ciclos × 2 notas, cada una con su note-off.
    CHECK (countOn (msgs, 60) == 3);
    CHECK (countOn (msgs, 64) == 3);
    CHECK (msgs.size() == 12);

    std::vector<double> ons, offs;
    for (const auto& m : msgs)
        (m.on ? ons : offs).push_back (m.beat);

    REQUIRE (ons.size() == 6);
    CHECK_NEAR (ons[0], 0.0);
    CHECK_NEAR (ons[1], 2.0);
    CHECK_NEAR (ons[2], 4.0);
    CHECK_NEAR (ons[5], 10.0);

    REQUIRE (offs.size() == 6);
    CHECK_NEAR (offs[0], 1.0);
    CHECK_NEAR (offs[5], 11.0);
}

TEST ("secuenciador: el resultado no depende del tamaño de bloque")
{
    const auto schedule = twoNotes();

    for (const auto block : { 0.01, 0.37, 1.0, 3.3 })
    {
        engine::Sequencer seq;
        seq.setSchedule (&schedule);
        const auto msgs = play (seq, 0.0, 40.0, block);

        CHECK (countOn (msgs, 60) == 10);
        CHECK (countOn (msgs, 64) == 10);
    }
}

TEST ("secuenciador: una nota termina aunque el horario cambie a mitad")
{
    engine::Schedule a;
    a.events = { note (0.0, 3.0, 60) };
    a.cycleLengthBeats = 8.0;

    engine::Schedule b;
    b.events = { note (6.0, 1.0, 70) };
    b.cycleLengthBeats = 8.0;

    engine::Sequencer seq;
    seq.setSchedule (&a);
    auto msgs = play (seq, 0.0, 1.0, 0.25);

    seq.setSchedule (&b);  // edición en vivo en el pulso 1
    const auto rest = play (seq, 1.0, 8.0, 0.25);
    msgs.insert (msgs.end(), rest.begin(), rest.end());

    bool sixtyOff = false;
    for (const auto& m : msgs)
        if (! m.on && m.pitch == 60)
        {
            sixtyOff = true;
            CHECK_NEAR (m.beat, 3.0);
        }

    CHECK (sixtyOff);
    CHECK (countOn (msgs, 70) == 1);
}

TEST ("secuenciador: al cambiar de horario sigue desde la posición del ciclo")
{
    const auto schedule = twoNotes();

    engine::Sequencer seq;
    const auto msgs = [&]
    {
        seq.setSchedule (&schedule);
        std::vector<Message> out;
        // Empieza en el pulso 5: ya pasó el do del segundo ciclo (pulso 4).
        seq.process (5.0, 7.0, [&out] (double beat, int pitch, int, bool on) { out.push_back ({ beat, pitch, on }); });
        return out;
    }();

    REQUIRE (! msgs.empty());
    CHECK (msgs[0].on);
    CHECK (msgs[0].pitch == 64);
    CHECK_NEAR (msgs[0].beat, 6.0);
}

TEST ("secuenciador: una nota repetida se apaga antes de volver a sonar")
{
    engine::Schedule s;
    s.events = { note (0.0, 3.0, 60) };  // dura más que el ciclo
    s.cycleLengthBeats = 2.0;

    engine::Sequencer seq;
    seq.setSchedule (&s);
    const auto msgs = play (seq, 0.0, 4.5, 0.5);

    // on 0 · off 2 · on 2 · off 4 · on 4
    REQUIRE (msgs.size() == 5);
    CHECK (msgs[1].on == false);
    CHECK_NEAR (msgs[1].beat, 2.0);
    CHECK (msgs[2].on);
    CHECK_NEAR (msgs[2].beat, 2.0);
    CHECK (seq.getNumActiveNotes() == 1);
}

TEST ("secuenciador: apagar todo deja cero notas activas")
{
    engine::Schedule s;
    s.events = { { 0.0, 4.0, { 60, 64, 67 }, 100, 0 } };  // un acorde largo
    s.cycleLengthBeats = 8.0;

    engine::Sequencer seq;
    seq.setSchedule (&s);
    play (seq, 0.0, 1.0, 1.0);
    CHECK (seq.getNumActiveNotes() == 3);

    int offs = 0;
    seq.allNotesOff (1.0, [&offs] (double, int, int, bool on) { offs += on ? 0 : 1; });
    CHECK (offs == 3);
    CHECK (seq.getNumActiveNotes() == 0);
}

TEST ("secuenciador: sin horario no emite nada")
{
    engine::Sequencer seq;
    CHECK (play (seq, 0.0, 10.0, 1.0).empty());

    engine::Schedule empty;
    empty.cycleLengthBeats = 4.0;
    seq.setSchedule (&empty);
    CHECK (play (seq, 0.0, 10.0, 1.0).empty());
}
