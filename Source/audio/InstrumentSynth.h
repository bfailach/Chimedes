#pragma once

#include <cmath>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "model/Instruments.h"

namespace chimedes::audio
{

// Un instrumento integrado. Responde solo a su canal MIDI, que es su id.
struct InstrumentSound : public juce::SynthesiserSound
{
    explicit InstrumentSound (int instrumentId) : instrument (instrumentId) {}

    bool appliesToNote (int) override            { return true; }
    bool appliesToChannel (int channel) override { return channel == instrument; }

    const int instrument;
};

// Voz que sintetiza cualquiera de los instrumentos integrados. El instrumento se fija
// al empezar cada nota, según el sonido (canal) que la dispara. No asigna memoria en
// el hilo de audio: el búfer de la cuerda pulsada se reserva al fijar la frecuencia
// de muestreo.
class InstrumentVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<InstrumentSound*> (sound) != nullptr;
    }

    void setCurrentPlaybackSampleRate (double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);

        if (newRate > 0.0)
        {
            envelope.setSampleRate (newRate);
            pluckBuffer.assign (static_cast<std::size_t> (newRate / 20.0) + 2, 0.0f);  // notas de 20 Hz o más
        }
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound* sound, int) override
    {
        const auto sampleRate = getSampleRate();
        instrument = static_cast<InstrumentSound*> (sound)->instrument;
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNote);

        phase = modPhase = detunePhase = 0.0;
        filterState = 0.0;
        samplesPlayed = 0;

        namespace ids = model::instruments;
        juce::ADSR::Parameters p;
        double gain = 0.18;

        switch (instrument)
        {
            case ids::electricPiano: p = { 0.002f, 1.6f, 0.0f, 0.35f }; gain = 0.22; break;
            case ids::bell:          p = { 0.001f, 3.0f, 0.0f, 1.2f };  gain = 0.15; break;
            case ids::pluck:         p = { 0.001f, 0.01f, 1.0f, 0.15f }; gain = 0.55; startPluck (sampleRate); break;
            case ids::pad:           p = { 0.35f, 0.5f, 0.8f, 0.9f };   gain = 0.30; break;
            case ids::bass:          p = { 0.003f, 0.3f, 0.7f, 0.12f }; gain = 0.42; break;
            case ids::organ:
            default:                 p = { 0.005f, 0.25f, 0.6f, 0.3f }; gain = 0.18; break;
        }

        level = gain * velocity;
        envelope.setParameters (p);
        envelope.reset();
        envelope.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            envelope.noteOff();
        }
        else
        {
            envelope.reset();
            clearCurrentNote();
        }
    }

    using juce::SynthesiserVoice::renderNextBlock;  // conserva la versión en double

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        const auto sampleRate = getSampleRate();
        const auto twoPi = juce::MathConstants<double>::twoPi;
        const auto delta = twoPi * frequency / sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto t = static_cast<double> (samplesPlayed++) / sampleRate;
            const auto sample = static_cast<float> (nextSample (t, delta, sampleRate) * level) * envelope.getNextSample();

            for (int channel = 0; channel < output.getNumChannels(); ++channel)
                output.addSample (channel, startSample + i, sample);

            if (! envelope.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    // Una muestra del instrumento actual, sin envolvente ni ganancia.
    double nextSample (double t, double delta, double sampleRate)
    {
        namespace ids = model::instruments;
        const auto twoPi = juce::MathConstants<double>::twoPi;
        double out = 0.0;

        switch (instrument)
        {
            case ids::electricPiano:
            {
                // FM 1:1 con un índice que cae rápido: el ataque metálico de un piano de púas.
                const auto index = 1.8 * std::exp (-6.0 * t) + 0.2;
                out = std::sin (phase + index * std::sin (modPhase));
                advance (modPhase, delta);
                break;
            }

            case ids::bell:
            {
                // FM 1:3,5, inarmónica, con un brillo que se apaga despacio.
                const auto index = 3.0 * std::exp (-2.5 * t) + 0.1;
                out = std::sin (phase + index * std::sin (modPhase));
                advance (modPhase, delta * 3.5);
                break;
            }

            case ids::pluck:
            {
                // Karplus-Strong: un búfer de ruido que se filtra a sí mismo en cada vuelta.
                const auto current = pluckBuffer[pluckIndex];
                const auto next = pluckBuffer[(pluckIndex + 1) % pluckLength];
                pluckBuffer[pluckIndex] = 0.5f * (current + next) * 0.996f;
                pluckIndex = (pluckIndex + 1) % pluckLength;
                out = current;
                break;
            }

            case ids::pad:
            {
                // Dos dientes de sierra ligeramente desafinados, suavizados con un filtro.
                const auto sawA = phase / juce::MathConstants<double>::pi - 1.0;
                const auto sawB = detunePhase / juce::MathConstants<double>::pi - 1.0;
                out = lowPass (0.5 * (sawA + sawB), 1400.0, sampleRate);
                advance (detunePhase, delta * 1.004);
                break;
            }

            case ids::bass:
            {
                // Sierra muy filtrada más una senoidal: redondo y con cuerpo.
                const auto saw = phase / juce::MathConstants<double>::pi - 1.0;
                out = 0.6 * lowPass (saw, 600.0, sampleRate) + 0.6 * std::sin (phase);
                break;
            }

            case ids::organ:
            default:
                out = std::sin (phase) + 0.30 * std::sin (2.0 * phase) + 0.12 * std::sin (3.0 * phase);
                break;
        }

        advance (phase, delta);
        juce::ignoreUnused (twoPi);
        return out;
    }

    static void advance (double& p, double delta) noexcept
    {
        p += delta;
        if (p >= juce::MathConstants<double>::twoPi)
            p -= juce::MathConstants<double>::twoPi;
    }

    // Filtro paso bajo de un polo.
    double lowPass (double input, double cutoff, double sampleRate) noexcept
    {
        const auto a = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * cutoff / sampleRate);
        filterState += a * (input - filterState);
        return filterState;
    }

    void startPluck (double sampleRate)
    {
        pluckLength = juce::jlimit<std::size_t> (2, pluckBuffer.size(),
                                                 static_cast<std::size_t> (std::round (sampleRate / frequency)));
        pluckIndex = 0;

        for (std::size_t i = 0; i < pluckLength; ++i)
            pluckBuffer[i] = random.nextFloat() * 2.0f - 1.0f;

        // Dos pasadas de media móvil: un ataque más cálido que el ruido blanco puro.
        for (int pass = 0; pass < 2; ++pass)
            for (std::size_t i = 0; i < pluckLength; ++i)
                pluckBuffer[i] = 0.5f * (pluckBuffer[i] + pluckBuffer[(i + 1) % pluckLength]);
    }

    juce::ADSR envelope;
    juce::Random random;

    int instrument = model::instruments::organ;
    double frequency = 440.0;
    double level = 0.0;
    double phase = 0.0, modPhase = 0.0, detunePhase = 0.0;
    double filterState = 0.0;
    std::int64_t samplesPlayed = 0;

    std::vector<float> pluckBuffer;
    std::size_t pluckLength = 2, pluckIndex = 0;
};

} // namespace chimedes::audio
