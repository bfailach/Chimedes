#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "engine/Schedule.h"
#include "engine/Sequencer.h"
#include "audio/InstrumentSynth.h"
#include "audio/SampleSynth.h"

namespace chimedes::audio
{

// Plays the tracks' schedules. It is a JUCE AudioSource: the same class serves the
// sound card and offline rendering (WAV export).
//
// Threading rules:
//   - publishSchedule, play, pause, stop, setBpm, collectGarbage and preview notes
//     (previewNoteOn/Off): UI thread only.
//   - getNextAudioBlock: audio thread only. It neither allocates nor locks.
//   - Old schedules are never freed on the audio thread: collectGarbage does it on
//     the UI thread once audio can no longer be using them.
class AudioEngine : public juce::AudioSource
{
public:
    static constexpr int maxTracks = 16;
    static constexpr int voicesPerTrack = 24;

    AudioEngine();
    ~AudioEngine() override;

    // juce::AudioSource
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& block) override;

    // --- UI thread -----------------------------------------------------------

    // Publishes a track's schedule (0..maxTracks-1). nullptr silences it.
    // Takes effect on the next audio block, keeping the position in the cycle.
    void publishSchedule (int slot, std::shared_ptr<const engine::Schedule> schedule);

    void play() noexcept;
    void pause() noexcept;
    void stop() noexcept;

    // Back to beat 0 and keep playing (stop + play in the same audio block).
    void restart() noexcept;
    bool isPlaying() const noexcept { return playing.load(); }

    void setBpm (double newBpm) noexcept;
    double getBpm() const noexcept { return bpm.load(); }

    // Beats elapsed since playback started (for the timeline).
    double getPositionBeats() const noexcept { return positionBeats.load(); }

    // Frees retired schedules that audio no longer uses. Call it periodically.
    void collectGarbage();

    // Preview notes (playing the panel's keyboard). They sound even when not
    // playing and do not affect the tracks. If the queue is full, they are dropped.
    void previewNoteOn (int pitch, int velocity, int instrument) noexcept;
    void previewNoteOff (int pitch, int instrument) noexcept;
    void previewAllNotesOff() noexcept;

    // Replaces the loaded samples (instruments 7..16) on every track and in the
    // preview notes. Notes still sounding with a removed sample end normally.
    // UI thread only.
    void setSamples (const std::vector<SampleData>& samples);

private:
    struct TrackSlot
    {
        std::atomic<const engine::Schedule*> pending { nullptr };  // written by the UI
        std::atomic<const engine::Schedule*> inUse { nullptr };    // written by audio
        engine::Sequencer sequencer;
        InstrumentSynthesiser synth;
        juce::MidiBuffer midi;
    };

    struct Published
    {
        std::shared_ptr<const engine::Schedule> schedule;
        int slot = 0;
        int retiredTicks = -1;  // -1 = current
    };

    struct PreviewMessage
    {
        int pitch = 0;          // -1 = all notes off
        int velocity = 0;       // 0 = note-off
        int instrument = 1;
    };

    void pushPreview (const PreviewMessage& message) noexcept;
    void collectPreview();

    void renderTrack (TrackSlot& slot, juce::AudioBuffer<float>& buffer,
                      int startSample, int numSamples, double fromBeat, double beatsPerSample);

    std::array<std::unique_ptr<TrackSlot>, maxTracks> slots;
    std::vector<Published> published;  // UI only

    std::atomic<bool> playing { false };
    std::atomic<bool> pauseRequested { false };
    std::atomic<bool> stopRequested { false };
    std::atomic<double> bpm { 120.0 };
    std::atomic<double> positionBeats { 0.0 };

    double sampleRate = 44100.0;
    double position = 0.0;  // audio only

    // Everything is mixed here first: channels 0-1 dry, 2-3 the reverb send.
    // Blocks longer than it are rendered in pieces.
    juce::AudioBuffer<float> mix { 4, 1024 };
    juce::Reverb reverb;
    double sendCoefficient = 0.98;          // high-pass on the send, so the room is not boomy
    std::array<double, 2> sendState {}, sendPrevious {};

    // Lock-free queue from the UI to audio (one producer, one consumer).
    static constexpr int previewQueueSize = 256;
    juce::AbstractFifo previewFifo { previewQueueSize };
    std::array<PreviewMessage, previewQueueSize> previewQueue {};
    InstrumentSynthesiser previewSynth;
    juce::MidiBuffer previewMidi;

    static constexpr int sampleVoicesPerTrack = 12;

    // Removed samples: kept for a while so they are never freed on the audio
    // thread (a voice may still be finishing them).
    struct RetiredSamples
    {
        std::vector<SampleData> samples;
        int ticks = 0;
    };
    std::vector<SampleData> currentSamples;
    std::vector<RetiredSamples> retiredSamples;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace chimedes::audio
