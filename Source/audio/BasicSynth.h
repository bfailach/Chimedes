#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace chimedes::audio
{

// Sonido único que acepta cualquier nota y canal.
struct BasicSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// Voz sencilla: seno con dos armónicos suaves y envolvente ADSR.
// Suena a un órgano suave; suficiente para la fase 1. Los instrumentos y las
// muestras llegan en la fase 3.
class BasicVoice : public juce::SynthesiserVoice
{
public:
    BasicVoice()
    {
        envelopeParams.attack = 0.005f;
        envelopeParams.decay = 0.25f;
        envelopeParams.sustain = 0.6f;
        envelopeParams.release = 0.3f;
    }

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<BasicSound*> (sound) != nullptr;
    }

    void setCurrentPlaybackSampleRate (double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);

        if (newRate > 0.0)
        {
            envelope.setSampleRate (newRate);
            envelope.setParameters (envelopeParams);
        }
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        phase = 0.0;
        phaseDelta = juce::MathConstants<double>::twoPi
                     * juce::MidiMessage::getMidiNoteInHertz (midiNote) / getSampleRate();
        level = 0.18f * velocity;

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

        for (int i = 0; i < numSamples; ++i)
        {
            const auto wave = std::sin (phase)
                            + 0.30 * std::sin (2.0 * phase)
                            + 0.12 * std::sin (3.0 * phase);

            const auto sample = static_cast<float> (wave) * level * envelope.getNextSample();

            for (int channel = 0; channel < output.getNumChannels(); ++channel)
                output.addSample (channel, startSample + i, sample);

            phase += phaseDelta;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;

            if (! envelope.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    juce::ADSR envelope;
    juce::ADSR::Parameters envelopeParams;
    double phase = 0.0;
    double phaseDelta = 0.0;
    float level = 0.0f;
};

} // namespace chimedes::audio
