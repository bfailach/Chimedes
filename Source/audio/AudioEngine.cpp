#include "audio/AudioEngine.h"

#include <algorithm>

#include "audio/InstrumentSynth.h"

namespace chimedes::audio
{

namespace
{
    // Receptor que descarta mensajes (al apagar notas, el sintetizador ya se encarga).
    constexpr auto ignoreMessages = [] (double, int, int, int, bool) {};
}

AudioEngine::AudioEngine()
{
    for (auto& slot : slots)
    {
        slot = std::make_unique<TrackSlot>();

        // Un sonido por instrumento, cada uno en su canal MIDI.
        for (int id = model::instruments::first; id <= model::instruments::count; ++id)
            slot->synth.addSound (new InstrumentSound (id));

        for (int i = 0; i < voicesPerTrack; ++i)
            slot->synth.addVoice (new InstrumentVoice());

        for (int i = 0; i < sampleVoicesPerTrack; ++i)
            slot->synth.addVoice (new SampleVoice());
    }

    for (int id = model::instruments::first; id <= model::instruments::count; ++id)
        previewSynth.addSound (new InstrumentSound (id));

    for (int i = 0; i < 16; ++i)
        previewSynth.addVoice (new InstrumentVoice());

    for (int i = 0; i < 8; ++i)
        previewSynth.addVoice (new SampleVoice());
}

AudioEngine::~AudioEngine() = default;

// ---------------------------------------------------------------------------
// Hilo de audio
// ---------------------------------------------------------------------------

void AudioEngine::prepareToPlay (int, double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& slot : slots)
    {
        slot->synth.setCurrentPlaybackSampleRate (newSampleRate);
        slot->midi.ensureSize (8192);  // para no asignar dentro del ciclo de audio
    }

    previewSynth.setCurrentPlaybackSampleRate (newSampleRate);
    previewMidi.ensureSize (4096);
}

void AudioEngine::releaseResources()
{
}

void AudioEngine::getNextAudioBlock (const juce::AudioSourceChannelInfo& block)
{
    block.clearActiveBufferRegion();

    if (stopRequested.exchange (false))
    {
        for (auto& slot : slots)
        {
            slot->sequencer.allNotesOff (position, ignoreMessages);
            slot->sequencer.rewind();
            slot->synth.allNotesOff (0, false);
        }

        position = 0.0;
        positionBeats.store (0.0);
    }
    else if (pauseRequested.exchange (false))
    {
        for (auto& slot : slots)
        {
            slot->sequencer.allNotesOff (position, ignoreMessages);
            slot->synth.allNotesOff (0, true);  // con cola de release
        }
    }

    // Toma el horario más reciente de cada pista y anuncia cuál está usando.
    for (auto& slot : slots)
    {
        const auto* schedule = slot->pending.load (std::memory_order_acquire);
        slot->sequencer.setSchedule (schedule);
        slot->inUse.store (schedule, std::memory_order_release);
    }

    const bool isRunning = playing.load();
    const auto beatsPerSample = bpm.load() / (60.0 * sampleRate);

    for (auto& slot : slots)
    {
        if (isRunning)
        {
            renderTrack (*slot, *block.buffer, block.startSample, block.numSamples, position, beatsPerSample);
        }
        else
        {
            slot->midi.clear();  // sin notas nuevas, pero las colas de release siguen sonando
            slot->synth.renderNextBlock (*block.buffer, slot->midi, block.startSample, block.numSamples);
        }
    }

    if (isRunning)
    {
        position += block.numSamples * beatsPerSample;
        positionBeats.store (position);
    }

    renderPreview (*block.buffer, block.startSample, block.numSamples);
}

void AudioEngine::renderPreview (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    previewMidi.clear();

    const auto scope = previewFifo.read (previewFifo.getNumReady());
    const auto take = [&] (int start, int count)
    {
        for (int i = start; i < start + count; ++i)
        {
            const auto& m = previewQueue[static_cast<std::size_t> (i)];
            const auto channel = std::clamp (m.instrument, 1, 16);

            if (m.pitch < 0)
                previewMidi.addEvent (juce::MidiMessage::allNotesOff (channel), startSample);
            else if (m.velocity > 0)
                previewMidi.addEvent (juce::MidiMessage::noteOn (channel, m.pitch, static_cast<juce::uint8> (std::clamp (m.velocity, 1, 127))), startSample);
            else
                previewMidi.addEvent (juce::MidiMessage::noteOff (channel, m.pitch), startSample);
        }
    };
    take (scope.startIndex1, scope.blockSize1);
    take (scope.startIndex2, scope.blockSize2);

    previewSynth.renderNextBlock (buffer, previewMidi, startSample, numSamples);
}

void AudioEngine::renderTrack (TrackSlot& slot, juce::AudioBuffer<float>& buffer,
                               int startSample, int numSamples, double fromBeat, double beatsPerSample)
{
    slot.midi.clear();

    const auto toBeat = fromBeat + numSamples * beatsPerSample;

    slot.sequencer.process (fromBeat, toBeat, [&] (double beat, int pitch, int velocity, int instrument, bool isNoteOn)
    {
        const auto offset = std::clamp (static_cast<int> ((beat - fromBeat) / beatsPerSample), 0, numSamples - 1);
        const auto channel = std::clamp (instrument, 1, 16);  // el canal MIDI elige el instrumento

        const auto message = isNoteOn
            ? juce::MidiMessage::noteOn (channel, pitch, static_cast<juce::uint8> (std::clamp (velocity, 1, 127)))
            : juce::MidiMessage::noteOff (channel, pitch);

        slot.midi.addEvent (message, startSample + offset);
    });

    slot.synth.renderNextBlock (buffer, slot.midi, startSample, numSamples);
}

// ---------------------------------------------------------------------------
// Hilo de la interfaz
// ---------------------------------------------------------------------------

void AudioEngine::publishSchedule (int slot, std::shared_ptr<const engine::Schedule> schedule)
{
    jassert (slot >= 0 && slot < maxTracks);

    for (auto& p : published)
        if (p.slot == slot && p.retiredTicks < 0)
            p.retiredTicks = 0;

    const auto* raw = schedule.get();

    if (schedule != nullptr)
        published.push_back ({ std::move (schedule), slot, -1 });

    slots[static_cast<std::size_t> (slot)]->pending.store (raw, std::memory_order_release);
}

void AudioEngine::setSamples (const std::vector<SampleData>& samples)
{
    const auto replace = [&samples] (juce::Synthesiser& synth)
    {
        for (int i = synth.getNumSounds(); --i >= 0;)
            if (dynamic_cast<SampleSound*> (synth.getSound (i).get()) != nullptr)
                synth.removeSound (i);

        for (const auto& s : samples)
            if (s.buffer != nullptr && s.buffer->getNumSamples() > 1)
                synth.addSound (new SampleSound (s));
    };

    for (auto& slot : slots)
        replace (slot->synth);
    replace (previewSynth);

    retiredSamples.push_back ({ std::move (currentSamples), 0 });
    currentSamples = samples;
}

void AudioEngine::collectGarbage()
{
    // Muestras quitadas hace un par de segundos: ninguna voz las puede estar usando.
    for (auto& r : retiredSamples)
        ++r.ticks;
    retiredSamples.erase (std::remove_if (retiredSamples.begin(), retiredSamples.end(),
                                          [] (const RetiredSamples& r) { return r.ticks > 8; }),
                          retiredSamples.end());

    // Un horario retirado se libera tras dos llamadas (cientos de milisegundos, mucho
    // más que un bloque de audio) y solo si el audio no lo está usando.
    for (auto& p : published)
        if (p.retiredTicks >= 0)
            ++p.retiredTicks;

    published.erase (std::remove_if (published.begin(), published.end(), [this] (const Published& p)
    {
        return p.retiredTicks >= 2
            && slots[static_cast<std::size_t> (p.slot)]->inUse.load (std::memory_order_acquire) != p.schedule.get();
    }), published.end());
}

void AudioEngine::play() noexcept
{
    playing.store (true);
}

void AudioEngine::pause() noexcept
{
    if (playing.exchange (false))
        pauseRequested.store (true);
}

void AudioEngine::stop() noexcept
{
    playing.store (false);
    stopRequested.store (true);
}

void AudioEngine::pushPreview (const PreviewMessage& message) noexcept
{
    const auto scope = previewFifo.write (1);
    if (scope.blockSize1 > 0)
        previewQueue[static_cast<std::size_t> (scope.startIndex1)] = message;
    else if (scope.blockSize2 > 0)
        previewQueue[static_cast<std::size_t> (scope.startIndex2)] = message;
}

void AudioEngine::previewNoteOn (int pitch, int velocity, int instrument) noexcept
{
    pushPreview ({ std::clamp (pitch, 0, 127), std::max (1, velocity), instrument });
}

void AudioEngine::previewNoteOff (int pitch, int instrument) noexcept
{
    pushPreview ({ std::clamp (pitch, 0, 127), 0, instrument });
}

void AudioEngine::previewAllNotesOff() noexcept
{
    for (int id = model::instruments::first; id <= model::instruments::count; ++id)
        pushPreview ({ -1, 0, id });
}

void AudioEngine::setBpm (double newBpm) noexcept
{
    bpm.store (std::clamp (newBpm, 20.0, 400.0));
}

} // namespace chimedes::audio
