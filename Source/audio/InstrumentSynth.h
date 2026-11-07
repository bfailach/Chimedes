#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "model/Instruments.h"

namespace chimedes::audio
{

// All built-in instruments share one MIDI channel in the engine; a program
// change (the instrument id) right before each note-on and note-off says which
// one plays. That leaves channels 7..16 for samples and allows any number of
// built-ins.
inline constexpr int builtInChannel = 1;

// The MIDI channel an instrument is played on inside the engine.
inline int engineChannelFor (int instrument) noexcept
{
    const auto id = static_cast<model::InstrumentId> (instrument);
    return model::instruments::isSample (id) ? instrument : builtInChannel;
}

// Every built-in instrument (it only answers to the built-in channel).
struct InstrumentSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override            { return true; }
    bool appliesToChannel (int channel) override { return channel == builtInChannel; }
};

namespace dsp
{
    inline constexpr double twoPi = juce::MathConstants<double>::twoPi;
    inline constexpr double pi = juce::MathConstants<double>::pi;

    // PolyBLEP correction: removes most of the aliasing of naive saw and square waves.
    inline double polyBlep (double t, double dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    // Oscillator with its phase in [0, 1).
    struct Osc
    {
        double phase = 0.0;

        void advance (double dt) noexcept
        {
            phase += dt;
            if (phase >= 1.0)
                phase -= 1.0;
        }

        double sine (double dt) noexcept
        {
            const auto v = std::sin (twoPi * phase);
            advance (dt);
            return v;
        }

        double saw (double dt) noexcept
        {
            const auto v = 2.0 * phase - 1.0 - polyBlep (phase, dt);
            advance (dt);
            return v;
        }

        double pulse (double dt, double width = 0.5) noexcept
        {
            auto v = phase < width ? 1.0 : -1.0;
            v += polyBlep (phase, dt);
            auto shifted = phase + (1.0 - width);
            if (shifted >= 1.0)
                shifted -= 1.0;
            v -= polyBlep (shifted, dt);
            advance (dt);
            return v;
        }
    };

    // State-variable filter (Simper's trapezoidal version): stable when its
    // cutoff is modulated quickly and at high resonance.
    struct Svf
    {
        double a1 = 0.0, a2 = 0.0, a3 = 0.0, k = 1.0;
        double ic1 = 0.0, ic2 = 0.0;
        double lp = 0.0, bp = 0.0;

        void set (double cutoff, double q, double sampleRate) noexcept
        {
            cutoff = std::clamp (cutoff, 20.0, 0.45 * sampleRate);
            const auto g = std::tan (pi * cutoff / sampleRate);
            k = 1.0 / std::max (0.3, q);
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void process (double v0) noexcept
        {
            const auto v3 = v0 - ic2;
            const auto v1 = a1 * ic1 + a2 * v3;
            const auto v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            lp = v2;
            bp = v1;
        }

        double lowPass (double v) noexcept   { process (v); return lp; }
        double bandPass (double v) noexcept  { process (v); return k * bp; }  // unity gain at the centre
        void reset() noexcept                { ic1 = ic2 = lp = bp = 0.0; }
    };

    // A decaying sinusoid computed by rotating a complex number: one multiply
    // per sample, no sin(). Used for bells, bars, tines and piano strings.
    struct Mode
    {
        double re = 0.0, im = 0.0, cr = 1.0, ci = 0.0;
        double left = 1.0, right = 1.0;

        void set (double amplitude, double frequency, double t60, double sampleRate, double pan) noexcept
        {
            const auto w = twoPi * frequency / sampleRate;
            const auto r = std::exp (-6.907755 / (std::max (0.01, t60) * sampleRate));  // -60 dB after t60
            re = amplitude;
            im = 0.0;
            cr = r * std::cos (w);
            ci = r * std::sin (w);
            left = 1.0 - std::max (0.0, pan);
            right = 1.0 + std::min (0.0, pan);
        }

        double next() noexcept
        {
            const auto nre = re * cr - im * ci;
            im = re * ci + im * cr;
            re = nre;
            return im;
        }
    };

    struct Noise
    {
        std::uint32_t state = 0x9e3779b9u;

        double next() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return static_cast<double> (state) * (2.0 / 4294967295.0) - 1.0;
        }
    };

    inline double cents (double c) noexcept { return std::pow (2.0, c / 1200.0); }
}

// Voice that synthesizes every built-in instrument, in stereo. It writes the dry
// sound to channels 0 and 1 and, if the buffer has four channels, a reverb send
// to channels 2 and 3 (the engine adds the shared reverb).
//
// It does not allocate on the audio thread: the string buffer is reserved when
// the sample rate is set.
class InstrumentVoice : public juce::SynthesiserVoice
{
public:
    // The synthesiser says which instrument the next note is, just before it starts.
    void setNextInstrument (int id) noexcept { nextInstrument = id; }
    int getInstrument() const noexcept       { return instrument; }

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
            filterEnvelope.setSampleRate (newRate);
            line.assign (static_cast<std::size_t> (newRate / 20.0) + 8, 0.0f);  // notes of 20 Hz or higher
        }
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        namespace ids = model::instruments;

        instrument = ids::isBuiltIn (static_cast<model::InstrumentId> (nextInstrument)) ? nextInstrument : static_cast<int> (ids::organ);
        note = midiNote;
        vel = juce::jlimit (0.0, 1.0, static_cast<double> (velocity));
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        sampleRate = getSampleRate();

        samplesPlayed = 0;
        silentSamples = 0;
        controlCounter = 0;
        numModes = 0;
        for (auto& o : osc) o = {};
        for (auto& f : filters) f.reset();
        noise.state = 0x9e3779b9u ^ (static_cast<std::uint32_t> (++noteCounter) * 2654435761u);
        lfoPhase = 0.0;
        decay = 1.0;
        fm = 0.0;

        setUp();

        envelope.reset();
        envelope.noteOn();
        filterEnvelope.reset();
        filterEnvelope.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            envelope.noteOff();
            filterEnvelope.noteOff();
        }
        else
        {
            envelope.reset();
            clearCurrentNote();
        }
    }

    using juce::SynthesiserVoice::renderNextBlock;  // keeps the double version

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        const auto channels = output.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            double l = 0.0, r = 0.0;
            renderFrame (l, r);
            ++samplesPlayed;

            const auto amp = level * envelope.getNextSample();
            l *= amp;
            r *= amp;

            const auto position = startSample + i;
            if (channels == 1)
            {
                output.addSample (0, position, static_cast<float> (0.5 * (l + r)));
            }
            else
            {
                output.addSample (0, position, static_cast<float> (l));
                output.addSample (1, position, static_cast<float> (r));
                if (channels >= 4)
                {
                    output.addSample (2, position, static_cast<float> (l * send));
                    output.addSample (3, position, static_cast<float> (r * send));
                }
            }

            // It ends when released, or when a decaying sound has died away.
            silentSamples = std::abs (l) + std::abs (r) < 1.0e-5 ? silentSamples + 1 : 0;
            if (! envelope.isActive()
                || (silentSamples > static_cast<int> (0.05 * sampleRate) && samplesPlayed > static_cast<std::int64_t> (0.3 * sampleRate)))
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    // --- Per-note setup --------------------------------------------------------

    void setUp()
    {
        namespace ids = model::instruments;
        using dsp::cents;

        const auto keyboardPan = juce::jlimit (-0.35, 0.35, (note - 64) / 60.0);
        pan = 0.0;
        vibratoDepth = 0.0;
        vibratoRate = 5.0;
        vibratoDelay = 0.3;
        scoop = 0.0;
        velocitySensitive = true;
        juce::ADSR::Parameters amp { 0.002f, 0.0f, 1.0f, 0.3f };
        juce::ADSR::Parameters filt { 0.001f, 0.3f, 0.0f, 0.3f };

        switch (instrument)
        {
            case ids::piano:
            {
                // Two slightly detuned strings per partial (the beating gives the
                // two-stage decay of a piano), stretched partials and a hammer thump.
                const auto bright = 0.55 - 0.35 * vel;
                const auto inharmonicity = 0.00012 * std::exp (0.03 * (note - 60));
                const auto t60 = juce::jlimit (0.7, 9.0, 9.0 * std::pow (2.0, -(note - 36) / 14.0));
                for (int n = 1; n <= 10; ++n)
                {
                    const auto f = n * frequency * std::sqrt (1.0 + inharmonicity * n * n);
                    const auto a = std::exp (-(n - 1) * bright) / n;
                    const auto life = t60 / (1.0 + 0.7 * (n - 1));
                    addMode (a * 0.6, f * cents (-0.9), life, keyboardPan - 0.15);
                    addMode (a * 0.6, f * cents (0.9), life * 0.8, keyboardPan + 0.15);
                }
                hammer = 0.35 * vel;
                hammerDecay = std::exp (-1.0 / (0.004 * sampleRate));
                filters[0].set (frequency * 3.0 + 1200.0, 0.8, sampleRate);
                amp = { 0.001f, 0.0f, 1.0f, 0.22f };
                level = 0.239;
                send = 0.2;
                break;
            }

            case ids::electricPiano:
            {
                // 1:1 FM (the tine's bark depends on velocity) plus a high "ping".
                fmIndex = 0.5 + 2.4 * vel * vel;
                fmIndexDecay = std::exp (-5.0 / sampleRate);
                decayMul = std::exp (-6.907755 / (juce::jlimit (1.2, 6.0, 6.0 * std::pow (2.0, -(note - 48) / 18.0)) * sampleRate));
                addMode (0.12 * vel, frequency * 14.0, 0.06, 0.0);
                tremoloRate = 4.2;
                tremoloDepth = 0.3;
                amp = { 0.002f, 0.0f, 1.0f, 0.3f };
                level = 0.28;
                send = 0.16;
                break;
            }

            case ids::harpsichord:
            {
                // Bright plucked string, plucked near its end.
                startString (juce::jlimit (0.8, 4.0, 3.5 * std::pow (2.0, -(note - 48) / 24.0)), 0.12, 0.9, 1.0 / 9.0);
                amp = { 0.001f, 0.0f, 1.0f, 0.12f };
                level = 0.444 * (0.8 + 0.2 * vel);   // barely velocity-sensitive
                velocitySensitive = false;
                send = 0.2;
                pan = keyboardPan;
                break;
            }

            case ids::musicBox:
            {
                // Clamped steel tine: very high, inharmonic partials that die quickly.
                addMode (1.0, frequency, 2.4, 0.0);
                addMode (0.22 * (0.5 + vel), frequency * 6.27, 0.5, 0.2);
                addMode (0.06 * vel, frequency * 17.55, 0.12, -0.2);
                hammer = 0.05 * vel;
                hammerDecay = std::exp (-1.0 / (0.001 * sampleRate));
                filters[0].set (6000.0, 0.7, sampleRate);
                amp = { 0.001f, 0.0f, 1.0f, 0.9f };
                level = 0.265;
                send = 0.32;
                pan = keyboardPan;
                break;
            }

            case ids::bell:
            {
                // Church-bell partials (hum, prime, minor-third tierce, quint, nominal…),
                // each as a doublet that beats slowly.
                struct P { double ratio, amp, life; };
                constexpr std::array<P, 9> bell { { { 0.5, 0.35, 7.0 }, { 1.0, 0.6, 4.5 }, { 1.19, 0.45, 3.5 },
                                                    { 1.5, 0.25, 2.2 }, { 2.0, 0.7, 2.8 }, { 2.74, 0.3, 1.6 },
                                                    { 3.0, 0.25, 1.3 }, { 4.07, 0.2, 0.8 }, { 5.37, 0.12, 0.5 } } };
                for (std::size_t i = 0; i < bell.size(); ++i)
                {
                    const auto a = bell[i].amp * (i < 3 ? 1.0 : 0.4 + 0.6 * vel);
                    const auto side = (i % 2 == 0 ? 0.25 : -0.25);
                    addMode (a * 0.55, frequency * bell[i].ratio, bell[i].life, side);
                    addMode (a * 0.45, frequency * bell[i].ratio * 1.0015, bell[i].life * 0.9, -side);
                }
                amp = { 0.001f, 0.0f, 1.0f, 1.6f };
                level = 0.2;
                send = 0.38;
                break;
            }

            case ids::marimba:
            {
                // Wooden bar tuned 1:4:10, with a soft mallet thump.
                const auto t60 = juce::jlimit (0.25, 1.6, 1.3 * std::pow (2.0, -(note - 48) / 18.0));
                addMode (1.0, frequency, t60, 0.0);
                addMode (0.3 * vel, frequency * 3.99, t60 * 0.3, 0.15);
                addMode (0.1 * vel, frequency * 9.85, t60 * 0.12, -0.15);
                hammer = 0.2 * vel;
                hammerDecay = std::exp (-1.0 / (0.003 * sampleRate));
                filters[0].set (frequency * 2.0 + 400.0, 0.7, sampleRate);
                amp = { 0.001f, 0.0f, 1.0f, 0.35f };
                level = 0.289;
                send = 0.18;
                pan = keyboardPan;
                break;
            }

            case ids::vibraphone:
            {
                // Metal bar 1:4:10 with a long ring and the motor's tremolo.
                const auto t60 = juce::jlimit (1.5, 6.0, 6.0 * std::pow (2.0, -(note - 53) / 24.0));
                addMode (1.0, frequency, t60, 0.0);
                addMode (0.22 * (0.4 + vel), frequency * 4.0, t60 * 0.35, 0.2);
                addMode (0.05 * vel, frequency * 10.0, t60 * 0.1, -0.2);
                tremoloRate = 5.5;
                tremoloDepth = 0.35;
                amp = { 0.001f, 0.0f, 1.0f, 1.0f };
                level = 0.251;
                send = 0.3;
                pan = keyboardPan;
                break;
            }

            case ids::kalimba:
            {
                addMode (1.0, frequency, 1.8, 0.0);
                addMode (0.3 * vel, frequency * 5.65, 0.25, 0.25);
                addMode (0.08 * vel, frequency * 13.3, 0.08, -0.25);
                hammer = 0.12 * vel;
                hammerDecay = std::exp (-1.0 / (0.002 * sampleRate));
                filters[0].set (frequency + 300.0, 0.7, sampleRate);
                amp = { 0.001f, 0.0f, 1.0f, 0.6f };
                level = 0.296;
                send = 0.24;
                pan = keyboardPan;
                break;
            }

            case ids::steelDrum:
            {
                // Octave and twelfth partials. The upper ones swell in after the strike
                // (a slow mode minus a fast one at the same frequency).
                addMode (1.0, frequency, 1.4, 0.0);
                addMode (0.55, frequency * 2.0, 1.0, 0.2);
                addMode (-0.55, frequency * 2.0, 0.05, 0.2);
                addMode (0.3 * (0.5 + vel), frequency * 3.0, 0.6, -0.2);
                addMode (-0.3 * (0.5 + vel), frequency * 3.0, 0.03, -0.2);
                addMode (0.25, frequency * 1.003, 1.2, -0.3);
                amp = { 0.004f, 0.0f, 1.0f, 0.5f };
                level = 0.218;
                send = 0.25;
                break;
            }

            case ids::pluck:
            {
                // Nylon-like string plucked a fifth of the way along.
                startString (juce::jlimit (0.6, 4.5, 4.5 * std::pow (2.0, -(note - 40) / 20.0)), 0.5, 0.35 + 0.6 * vel, 0.2);
                amp = { 0.001f, 0.0f, 1.0f, 0.15f };
                level = 0.523;
                send = 0.16;
                pan = keyboardPan;
                break;
            }

            case ids::harp:
            {
                startString (juce::jlimit (1.0, 7.0, 7.0 * std::pow (2.0, -(note - 36) / 18.0)), 0.5, 0.2 + 0.35 * vel, 1.0 / 6.0);
                amp = { 0.001f, 0.0f, 1.0f, 1.2f };
                level = 0.475;
                send = 0.32;
                pan = keyboardPan;
                break;
            }

            case ids::strings:
            {
                // Ensemble: three sawtooths per side, filtered, with delayed vibrato.
                detunes = { cents (-7), cents (3), cents (11), cents (-11), cents (-2), cents (8) };
                cutoffBase = juce::jlimit (600.0, 8000.0, frequency * (3.0 + 3.0 * vel));
                vibratoDepth = 0.0025;
                vibratoRate = 5.2;
                vibratoDelay = 0.25;
                amp = { static_cast<float> (0.25 - 0.15 * vel), 0.3f, 0.85f, 0.45f };
                level = 0.119;
                send = 0.32;
                break;
            }

            case ids::flute:
            {
                vibratoDepth = 0.004;
                vibratoRate = 5.0;
                vibratoDelay = 0.3;
                scoop = -0.004;
                filters[0].set (frequency * 2.0, 3.0, sampleRate);
                filters[1].set (frequency * 4.0 + 1500.0, 0.7, sampleRate);
                hammer = 0.25;   // the chiff at the start of each note
                hammerDecay = std::exp (-1.0 / (0.025 * sampleRate));
                amp = { 0.06f, 0.1f, 0.9f, 0.15f };
                level = 0.169;
                send = 0.26;
                break;
            }

            case ids::brass:
            {
                // Sawtooths whose filter opens with the attack (the brassy "blat").
                detunes = { cents (-4), cents (4), 1.0, 1.0, 1.0, 1.0 };
                scoop = -0.015;
                vibratoDepth = 0.002;
                vibratoDelay = 0.4;
                filt = { 0.03f, 0.25f, 0.45f, 0.2f };
                amp = { 0.03f, 0.2f, 0.85f, 0.15f };
                level = 0.245;
                send = 0.2;
                break;
            }

            case ids::choir:
            {
                // Sawtooths through the formants of an "ah", every voice with its own vibrato.
                detunes = { cents (-9), cents (1), cents (8), cents (-5), cents (4), cents (11) };
                const auto f1 = std::max (800.0, frequency * 1.1);
                for (int side = 0; side < 2; ++side)
                {
                    filters[static_cast<std::size_t> (side * 3 + 0)].set (f1, 9.0, sampleRate);
                    filters[static_cast<std::size_t> (side * 3 + 1)].set (std::max (1150.0, f1 * 1.4), 12.0, sampleRate);
                    filters[static_cast<std::size_t> (side * 3 + 2)].set (2900.0, 20.0, sampleRate);
                }
                vibratoDepth = 0.003;
                vibratoDelay = 0.15;
                choirBody = {};
                choirBodyCoefficient = 1.0 - std::exp (-dsp::twoPi * std::min (3000.0, frequency * 2.5) / sampleRate);
                for (std::size_t i = 0; i < 6; ++i)
                    voiceSteps[i] = frequency / sampleRate * detunes[i];
                amp = { 0.25f, 0.2f, 0.9f, 0.5f };
                level = 0.27;
                send = 0.42;
                break;
            }

            case ids::pad:
            {
                detunes = { cents (-12), cents (5), cents (-5), cents (12), 1.0, 1.0 };
                cutoffBase = juce::jlimit (500.0, 6000.0, frequency * 3.0 + 600.0);
                amp = { 0.6f, 1.0f, 0.85f, 1.2f };
                level = 0.111;
                send = 0.45;
                break;
            }

            case ids::lead:
            {
                detunes = { 1.0, cents (6), 1.0, 1.0, 1.0, 1.0 };
                vibratoDepth = 0.004;
                vibratoRate = 5.5;
                vibratoDelay = 0.35;
                filt = { 0.002f, 0.4f, 0.3f, 0.2f };
                amp = { 0.005f, 0.3f, 0.75f, 0.12f };
                level = 0.156;
                send = 0.18;
                break;
            }

            case ids::synthPluck:
            {
                detunes = { cents (-8), cents (8), 1.0, 1.0, 1.0, 1.0 };
                filterDecay = std::exp (-12.0 / sampleRate);
                amp = { 0.001f, 0.5f, 0.0f, 0.2f };
                level = 0.2;
                send = 0.3;
                pan = keyboardPan;
                break;
            }

            case ids::bass:
            {
                filterDecay = std::exp (-8.0 / sampleRate);
                amp = { 0.003f, 0.4f, 0.75f, 0.1f };
                level = 0.32;
                send = 0.04;
                break;
            }

            case ids::subBass:
            {
                amp = { 0.004f, 0.2f, 0.9f, 0.08f };
                level = 0.126;
                send = 0.0;
                break;
            }

            case ids::acidBass:
            {
                filterDecay = std::exp (-(5.0 + 5.0 * (1.0 - vel)) / sampleRate);
                amp = { 0.002f, 0.3f, 0.6f, 0.06f };
                level = 0.153;
                send = 0.06;
                break;
            }

            case ids::organ:
            default:
            {
                // Drawbars 16', 8', 5⅓', 4', 2⅔', 2' and 1', a key click and a slow
                // rotating-speaker shimmer. Organs barely respond to velocity.
                constexpr std::array<std::pair<double, double>, 7> drawbars { { { 0.5, 0.55 }, { 1.0, 1.0 }, { 1.5, 0.35 }, { 2.0, 0.55 },
                                                                               { 3.0, 0.25 }, { 4.0, 0.3 }, { 8.0, 0.12 } } };
                for (const auto& [ratio, gain] : drawbars)
                    addMode (0.5 * gain, frequency * ratio, 1.0e9, 0.0);   // they do not decay
                hammer = 0.08;
                hammerDecay = std::exp (-1.0 / (0.002 * sampleRate));
                filters[0].set (3000.0, 0.7, sampleRate);
                tremoloRate = 0.8;
                tremoloDepth = 0.22;
                amp = { 0.006f, 0.0f, 1.0f, 0.08f };
                level = 0.171 * (0.7 + 0.3 * vel);
                velocitySensitive = false;
                send = 0.22;
                break;
            }
        }

        if (velocitySensitive)
            level *= 0.15 + 0.85 * vel;

        envelope.setParameters (amp);
        filterEnvelope.setParameters (filt);
    }

    void addMode (double amplitude, double f, double t60, double modePan)
    {
        if (numModes >= modes.size() || f >= 0.45 * sampleRate)
            return;
        modes[numModes++].set (amplitude, f, t60, sampleRate, juce::jlimit (-1.0, 1.0, modePan));
    }

    // Karplus-Strong string with a fractional delay, so high notes stay in tune.
    //   t60: seconds to fade 60 dB; loss: 0..0.5 (0.5 = the warmest);
    //   brightness: 0..1 of the pluck; position: where it is plucked (0..0.5).
    void startString (double t60, double loss, double brightness, double position)
    {
        stringLoss = loss;
        stringDelay = juce::jlimit (2.0, static_cast<double> (line.size() - 4), sampleRate / frequency - loss);
        stringGain = std::pow (10.0, -3.0 / (t60 * frequency));
        stringPrevious = 0.0;

        const auto length = static_cast<std::size_t> (std::ceil (stringDelay)) + 2;
        std::fill (line.begin(), line.end(), 0.0f);

        // The string's shape when released: a triangle peaking where it is plucked
        // (warm, harmonics falling as 1/n²) plus noise for the brightness of the pick.
        brightness = juce::jlimit (0.0, 1.0, brightness);
        const auto apex = juce::jlimit (0.05, 0.5, position) * static_cast<double> (length);
        const auto smooth = 1.0 - juce::jlimit (0.05, 1.0, brightness);
        double state = 0.0, mean = 0.0;
        for (std::size_t i = 0; i < length; ++i)
        {
            const auto x = static_cast<double> (i);
            const auto triangle = x < apex ? x / apex : (static_cast<double> (length) - x) / (static_cast<double> (length) - apex);
            state = smooth * state + (1.0 - smooth) * noise.next();
            const auto v = (1.0 - 0.6 * brightness) * triangle + 0.6 * brightness * state * 2.0;
            line[i] = static_cast<float> (v);
            mean += v;
        }
        mean /= static_cast<double> (length);

        // Without DC and with a peak of 1, whatever the pitch and brightness.
        double peak = 1.0e-9;
        for (std::size_t j = 0; j < length; ++j)
        {
            line[j] = static_cast<float> (line[j] - mean);
            peak = std::max (peak, std::abs (static_cast<double> (line[j])));
        }
        for (std::size_t j = 0; j < length; ++j)
            line[j] = static_cast<float> (line[j] / peak);

        writeIndex = length;
    }

    double nextString() noexcept
    {
        const auto size = static_cast<double> (line.size());
        auto readPosition = static_cast<double> (writeIndex) - stringDelay;
        if (readPosition < 0.0)
            readPosition += size;

        const auto i0 = static_cast<std::size_t> (readPosition);
        const auto i1 = (i0 + 1) % line.size();
        const auto frac = readPosition - static_cast<double> (i0);
        const auto x = static_cast<double> (line[i0]) + frac * (static_cast<double> (line[i1]) - static_cast<double> (line[i0]));

        const auto y = stringGain * ((1.0 - stringLoss) * x + stringLoss * stringPrevious);
        stringPrevious = x;
        line[writeIndex] = static_cast<float> (y);
        writeIndex = (writeIndex + 1) % line.size();
        return y;
    }

    // --- Per-sample rendering --------------------------------------------------

    void addModes (double& l, double& r) noexcept
    {
        for (std::size_t i = 0; i < numModes; ++i)
        {
            const auto v = modes[i].next();
            l += v * modes[i].left;
            r += v * modes[i].right;
        }
    }

    double vibrato (double t) noexcept
    {
        lfoPhase += vibratoRate / sampleRate;
        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;
        const auto depth = vibratoDepth * juce::jlimit (0.0, 1.0, (t - vibratoDelay) / 0.4);
        return 1.0 + depth * std::sin (dsp::twoPi * lfoPhase);
    }

    void tremolo (double& l, double& r) noexcept
    {
        lfoPhase += tremoloRate / sampleRate;
        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;
        const auto s = std::sin (dsp::twoPi * lfoPhase);
        l *= 1.0 - tremoloDepth * s;
        r *= 1.0 + tremoloDepth * s;
    }

    double burst() noexcept
    {
        const auto v = hammer * noise.next();
        hammer *= hammerDecay;
        return v;
    }

    void renderFrame (double& l, double& r) noexcept
    {
        namespace ids = model::instruments;
        const auto t = static_cast<double> (samplesPlayed) / sampleRate;
        const auto dt = frequency / sampleRate;
        const bool control = (controlCounter++ & 15) == 0;   // filter updates every 16 samples

        switch (instrument)
        {
            case ids::piano:
            {
                addModes (l, r);
                const auto thump = filters[0].bandPass (burst());
                l += thump;
                r += thump;
                break;
            }

            case ids::electricPiano:
            {
                fmIndex *= fmIndexDecay;
                const auto modulator = osc[1].sine (dt);
                const auto carrier = std::sin (dsp::twoPi * osc[0].phase + (fmIndex + 0.25) * modulator);
                osc[0].advance (dt);
                decay *= decayMul;
                const auto v = carrier * decay;
                l = r = v;
                addModes (l, r);
                tremolo (l, r);
                break;
            }

            case ids::harpsichord:
            case ids::pluck:
            case ids::harp:
                l = r = nextString();
                break;

            case ids::musicBox:
            case ids::marimba:
            case ids::kalimba:
            {
                addModes (l, r);
                const auto thump = filters[0].lowPass (burst());
                l += thump;
                r += thump;
                break;
            }

            case ids::bell:
            case ids::steelDrum:
                addModes (l, r);
                break;

            case ids::vibraphone:
            {
                addModes (l, r);
                lfoPhase += tremoloRate / sampleRate;
                if (lfoPhase >= 1.0)
                    lfoPhase -= 1.0;
                l *= 1.0 - tremoloDepth * (0.5 + 0.5 * std::sin (dsp::twoPi * lfoPhase));
                r *= 1.0 - tremoloDepth * (0.5 + 0.5 * std::sin (dsp::twoPi * lfoPhase + 0.6));
                break;
            }

            case ids::strings:
            {
                const auto vib = vibrato (t);
                if (control)
                {
                    filters[0].set (cutoffBase, 0.6, sampleRate);
                    filters[1].set (cutoffBase * 1.05, 0.6, sampleRate);
                }
                double a = 0.0, b = 0.0;
                for (std::size_t i = 0; i < 3; ++i)     a += osc[i].saw (dt * vib * detunes[i]);
                for (std::size_t i = 3; i < 6; ++i)     b += osc[i].saw (dt * vib * detunes[i] * (1.0 - 0.0006 * std::sin (dsp::twoPi * lfoPhase)));
                l = filters[0].lowPass (a);
                r = filters[1].lowPass (b);
                break;
            }

            case ids::flute:
            {
                scoop *= 0.9995;
                const auto vib = vibrato (t);
                const auto step = dt * vib * (1.0 + scoop);
                const auto p = dsp::twoPi * osc[0].phase;
                const auto over = 0.1 + 0.15 * vel;
                auto v = std::sin (p) + over * std::sin (2.0 * p) + 0.05 * std::sin (3.0 * p);
                osc[0].advance (step);
                v *= 1.0 + 0.06 * (vib - 1.0) / std::max (1.0e-9, vibratoDepth);   // a little breath pulse with the vibrato
                const auto breath = filters[0].bandPass (noise.next()) * 0.035 + filters[1].lowPass (burst()) * 0.4;
                l = r = 0.8 * v + breath;
                break;
            }

            case ids::brass:
            {
                scoop *= 0.9993;
                const auto vib = vibrato (t);
                const auto f = filterEnvelope.getNextSample();
                if (control)
                    filters[0].set (frequency * (1.2 + (2.0 + 6.0 * vel) * f), 0.9, sampleRate);
                const auto step = dt * vib * (1.0 + scoop);
                const auto v = 0.5 * (osc[0].saw (step * detunes[0]) + osc[1].saw (step * detunes[1]));
                const auto out = filters[0].lowPass (v);
                l = out * 1.05;
                r = out * 0.95;
                break;
            }

            case ids::choir:
            {
                if (control)
                {
                    // Each singer's vibrato, at its own rate (updated every 16 samples).
                    const auto ramp = juce::jlimit (0.0, 1.0, (t - vibratoDelay) / 0.5);
                    for (std::size_t i = 0; i < 6; ++i)
                    {
                        const auto rate = 4.6 + 0.35 * static_cast<double> (i);
                        voiceSteps[i] = dt * detunes[i] * (1.0 + vibratoDepth * ramp * std::sin (dsp::twoPi * rate * t + static_cast<double> (i)));
                    }
                }
                double sides[2] {};
                for (std::size_t i = 0; i < 6; ++i)
                    sides[i / 3] += osc[i].saw (voiceSteps[i]);
                for (int side = 0; side < 2; ++side)
                {
                    const auto source = sides[side] + 0.15 * noise.next();
                    auto& f = filters;
                    const auto base = static_cast<std::size_t> (side * 3);
                    // The formants, plus some of the source low-passed: the chest of the voice,
                    // so low notes keep their fundamental.
                    auto& chest = choirBody[static_cast<std::size_t> (side)];
                    chest += choirBodyCoefficient * (sides[side] - chest);
                    const auto v = f[base].bandPass (source) + 0.5 * f[base + 1].bandPass (source)
                                 + 0.25 * f[base + 2].bandPass (source) + 0.3 * chest;
                    (side == 0 ? l : r) = v;
                }
                break;
            }

            case ids::pad:
            {
                lfoPhase += 0.15 / sampleRate;
                if (lfoPhase >= 1.0)
                    lfoPhase -= 1.0;
                if (control)
                {
                    const auto c = cutoffBase * (1.0 + 0.3 * std::sin (dsp::twoPi * lfoPhase));
                    filters[0].set (c, 0.8, sampleRate);
                    filters[1].set (c * 1.1, 0.8, sampleRate);
                }
                const auto sub = 0.3 * osc[4].sine (dt * 0.5);
                const auto a = osc[0].saw (dt * detunes[0]) + osc[1].saw (dt * detunes[1]);
                const auto b = osc[2].saw (dt * detunes[2]) + osc[3].saw (dt * detunes[3]);
                l = filters[0].lowPass (a) + sub;
                r = filters[1].lowPass (b) + sub;
                break;
            }

            case ids::lead:
            {
                const auto vib = vibrato (t);
                const auto f = filterEnvelope.getNextSample();
                if (control)
                    filters[0].set (frequency * (2.5 + 4.0 * vel) + 3000.0 * vel * f, 2.0, sampleRate);
                const auto step = dt * vib;
                const auto v = 0.5 * osc[0].pulse (step, 0.42) + 0.5 * osc[1].saw (step * detunes[1]);
                const auto out = std::tanh (1.5 * filters[0].lowPass (v)) / std::tanh (1.5);
                l = out;
                r = out;
                break;
            }

            case ids::synthPluck:
            {
                fm = samplesPlayed == 0 ? 1.0 : fm * filterDecay;
                if (control)
                {
                    const auto c = frequency * 1.2 + (frequency * 12.0 + 2500.0) * vel * fm;
                    filters[0].set (c, 1.6, sampleRate);
                    filters[1].set (c, 1.6, sampleRate);
                }
                l = filters[0].lowPass (osc[0].saw (dt * detunes[0]));
                r = filters[1].lowPass (osc[1].saw (dt * detunes[1]));
                break;
            }

            case ids::bass:
            {
                fm = samplesPlayed == 0 ? 1.0 : fm * filterDecay;
                if (control)
                    filters[0].set (frequency * (2.0 + 5.0 * vel * fm) + 120.0, 0.8, sampleRate);
                const auto v = filters[0].lowPass (osc[0].saw (dt)) + 0.6 * osc[1].sine (dt);
                l = r = v;
                break;
            }

            case ids::subBass:
            {
                const auto v = std::tanh (1.4 * osc[0].sine (dt)) / std::tanh (1.4);
                l = r = v;
                break;
            }

            case ids::acidBass:
            {
                fm = samplesPlayed == 0 ? 1.0 : fm * filterDecay;
                if (control)
                    filters[0].set (frequency + (200.0 + 3200.0 * vel) * fm, 7.0, sampleRate);
                const auto v = std::tanh (2.0 * filters[0].lowPass (osc[0].saw (dt)));
                l = r = v;
                break;
            }

            case ids::organ:
            default:
            {
                addModes (l, r);
                const auto click = filters[0].lowPass (burst());
                l += click;
                r += click;
                tremolo (l, r);
                break;
            }
        }

        if (std::abs (pan) > 1.0e-9)
        {
            l *= 1.0 - std::max (0.0, pan);
            r *= 1.0 + std::min (0.0, pan);
        }
    }

    juce::ADSR envelope, filterEnvelope;
    dsp::Noise noise;
    static inline std::uint32_t noteCounter = 0;

    int nextInstrument = model::instruments::organ;
    int instrument = model::instruments::organ;
    int note = 60;
    double vel = 0.8, frequency = 440.0, sampleRate = 44100.0;
    double level = 0.0, send = 0.0, pan = 0.0;
    bool velocitySensitive = true;
    std::int64_t samplesPlayed = 0;
    int silentSamples = 0;
    unsigned controlCounter = 0;

    std::array<dsp::Osc, 6> osc {};
    std::array<double, 6> detunes { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
    std::array<dsp::Svf, 6> filters {};
    std::array<dsp::Mode, 24> modes {};
    std::size_t numModes = 0;

    double lfoPhase = 0.0, vibratoDepth = 0.0, vibratoRate = 5.0, vibratoDelay = 0.3;
    double tremoloRate = 0.0, tremoloDepth = 0.0;
    double scoop = 0.0;
    double hammer = 0.0, hammerDecay = 0.0;
    double fmIndex = 0.0, fmIndexDecay = 1.0, decay = 1.0, decayMul = 1.0;
    double fm = 0.0, filterDecay = 1.0;     // exponential filter envelope
    double cutoffBase = 1000.0;
    std::array<double, 6> voiceSteps {};
    std::array<double, 2> choirBody {};
    double choirBodyCoefficient = 0.1;

    std::vector<float> line;
    std::size_t writeIndex = 0;
    double stringDelay = 100.0, stringGain = 0.99, stringLoss = 0.5, stringPrevious = 0.0;
};

// Synthesiser that plays every built-in on the built-in channel. The program
// change before each note says which instrument it is, so a note-off only stops
// that instrument (two instruments can hold the same pitch at once). Other
// channels (samples) work as usual.
class InstrumentSynthesiser : public juce::Synthesiser
{
public:
    void handleProgramChange (int channel, int program) override
    {
        if (channel == builtInChannel)
            currentProgram = program;
    }

    void noteOn (int channel, int midiNote, float velocity) override
    {
        if (channel != builtInChannel)
        {
            juce::Synthesiser::noteOn (channel, midiNote, velocity);
            return;
        }

        const juce::ScopedLock sl (lock);

        for (auto* sound : sounds)
        {
            if (! sound->appliesToNote (midiNote) || ! sound->appliesToChannel (channel))
                continue;

            // The same note of the same instrument is retriggered.
            for (auto* voice : voices)
                if (auto* v = dynamic_cast<InstrumentVoice*> (voice))
                    if (v->getCurrentlyPlayingNote() == midiNote && v->isPlayingChannel (channel) && v->getInstrument() == currentProgram)
                        stopVoice (v, 1.0f, true);

            if (auto* voice = findFreeVoice (sound, channel, midiNote, isNoteStealingEnabled()))
            {
                if (auto* v = dynamic_cast<InstrumentVoice*> (voice))
                    v->setNextInstrument (currentProgram);
                startVoice (voice, sound, channel, midiNote, velocity);
            }
        }
    }

    void noteOff (int channel, int midiNote, float velocity, bool allowTailOff) override
    {
        if (channel != builtInChannel)
        {
            juce::Synthesiser::noteOff (channel, midiNote, velocity, allowTailOff);
            return;
        }

        const juce::ScopedLock sl (lock);

        for (auto* voice : voices)
            if (auto* v = dynamic_cast<InstrumentVoice*> (voice))
                if (v->getCurrentlyPlayingNote() == midiNote && v->isPlayingChannel (channel)
                    && v->getInstrument() == currentProgram && v->isKeyDown())
                {
                    v->setKeyDown (false);
                    stopVoice (v, velocity, allowTailOff);
                }
    }

private:
    int currentProgram = model::instruments::organ;
};

} // namespace chimedes::audio
