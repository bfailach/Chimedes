#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "engine/Schedule.h"
#include "engine/Sequencer.h"

namespace chimedes::audio
{

// Reproduce los horarios de las pistas. Es una AudioSource de JUCE: la misma clase
// sirve para la tarjeta de sonido y para el renderizado sin conexión (exportar WAV).
//
// Reglas de hilos:
//   - publishSchedule, play, pause, stop, setBpm y collectGarbage: solo desde la interfaz.
//   - getNextAudioBlock: solo desde el hilo de audio. No asigna, no bloquea.
//   - Los horarios viejos nunca se liberan en el hilo de audio: collectGarbage lo hace
//     en la interfaz cuando el audio ya no puede estar usándolos.
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

    // --- Hilo de la interfaz -------------------------------------------------

    // Publica el horario de una pista (0..maxTracks-1). nullptr la silencia.
    // Se aplica en el siguiente bloque de audio, conservando la posición del ciclo.
    void publishSchedule (int slot, std::shared_ptr<const engine::Schedule> schedule);

    void play() noexcept;
    void pause() noexcept;
    void stop() noexcept;
    bool isPlaying() const noexcept { return playing.load(); }

    void setBpm (double newBpm) noexcept;
    double getBpm() const noexcept { return bpm.load(); }

    // Pulsos transcurridos desde que empezó la reproducción (para la línea de tiempo).
    double getPositionBeats() const noexcept { return positionBeats.load(); }

    // Libera los horarios retirados que el audio ya no usa. Llamar periódicamente.
    void collectGarbage();

private:
    struct TrackSlot
    {
        std::atomic<const engine::Schedule*> pending { nullptr };  // escrito por la interfaz
        std::atomic<const engine::Schedule*> inUse { nullptr };    // escrito por el audio
        engine::Sequencer sequencer;
        juce::Synthesiser synth;
        juce::MidiBuffer midi;
    };

    struct Published
    {
        std::shared_ptr<const engine::Schedule> schedule;
        int slot = 0;
        int retiredTicks = -1;  // -1 = vigente
    };

    void renderTrack (TrackSlot& slot, juce::AudioBuffer<float>& buffer,
                      int startSample, int numSamples, double fromBeat, double beatsPerSample);

    std::array<std::unique_ptr<TrackSlot>, maxTracks> slots;
    std::vector<Published> published;  // solo la interfaz

    std::atomic<bool> playing { false };
    std::atomic<bool> pauseRequested { false };
    std::atomic<bool> stopRequested { false };
    std::atomic<double> bpm { 120.0 };
    std::atomic<double> positionBeats { 0.0 };

    double sampleRate = 44100.0;
    double position = 0.0;  // solo el audio

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace chimedes::audio
