#pragma once

#include <cmath>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>

namespace chimedes::audio
{

// Una muestra cargada por el usuario, ya decodificada (estéreo, en float).
struct SampleData
{
    int id = 0;                                              // canal MIDI (7..16)
    std::shared_ptr<const juce::AudioBuffer<float>> buffer;  // compartido entre todas las pistas
    double sampleRate = 44100.0;
    int rootNote = 60;
};

// Sonido de muestra: responde solo a su canal. No copia el audio, lo comparte.
struct SampleSound : public juce::SynthesiserSound
{
    explicit SampleSound (SampleData sampleData) : data (std::move (sampleData)) {}

    bool appliesToNote (int) override            { return true; }
    bool appliesToChannel (int channel) override { return channel == data.id; }

    const SampleData data;
};

// Voz que reproduce una muestra transpuesta (interpolación lineal) con una envolvente
// corta, para que no haga clic al empezar ni al soltar. No asigna memoria.
class SampleVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SampleSound*> (sound) != nullptr;
    }

    void setCurrentPlaybackSampleRate (double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
        if (newRate > 0.0)
            envelope.setSampleRate (newRate);
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound* sound, int) override
    {
        const auto& data = static_cast<SampleSound*> (sound)->data;
        buffer = data.buffer.get();

        // Paso por muestra de salida: transposición en semitonos y ajuste de frecuencias.
        step = std::pow (2.0, (midiNote - data.rootNote) / 12.0) * data.sampleRate / getSampleRate();
        position = 0.0;
        level = 0.7f * velocity;

        envelope.setParameters ({ 0.003f, 0.05f, 1.0f, 0.25f });
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
            finish();
        }
    }

    using juce::SynthesiserVoice::renderNextBlock;

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! isVoiceActive() || buffer == nullptr)
            return;

        const auto length = buffer->getNumSamples();
        const auto* left = buffer->getReadPointer (0);
        const auto* right = buffer->getReadPointer (buffer->getNumChannels() > 1 ? 1 : 0);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto index = static_cast<int> (position);
            if (index + 1 >= length)
            {
                finish();   // la muestra terminó antes que la nota
                return;
            }

            const auto frac = static_cast<float> (position - index);
            const auto gain = level * envelope.getNextSample();
            const auto l = (left[index] + frac * (left[index + 1] - left[index])) * gain;
            const auto r = (right[index] + frac * (right[index + 1] - right[index])) * gain;

            output.addSample (0, startSample + i, l);
            if (output.getNumChannels() > 1)
                output.addSample (1, startSample + i, r);

            position += step;

            if (! envelope.isActive())
            {
                finish();
                return;
            }
        }
    }

private:
    void finish()
    {
        buffer = nullptr;
        clearCurrentNote();
    }

    const juce::AudioBuffer<float>* buffer = nullptr;   // lo mantiene vivo el sonido
    double position = 0.0;
    double step = 1.0;
    float level = 0.0f;
    juce::ADSR envelope;
};

} // namespace chimedes::audio
