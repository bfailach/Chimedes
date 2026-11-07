#pragma once

#include <functional>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "audio/SampleSynth.h"
#include "model/Track.h"

namespace chimedes::audio
{

struct OfflineRenderSettings
{
    double sampleRate = 48000.0;
    int bitsPerSample = 24;
    double tailSeconds = 2.0;   // después del final, para que las notas se apaguen solas
};

// Renderiza el proyecto a un WAV con el mismo motor que suena en vivo (en una
// instancia aparte, sin tarjeta de sonido y más rápido que en tiempo real).
// Suenan las pistas no silenciadas durante `lengthBeats` pulsos al BPM del proyecto;
// después, las notas terminan con su cola. Si el resultado pasa de 0 dB, se baja el
// volumen entero para que no recorte.
//
// `progress` recibe de 0 a 1 y puede devolver false para cancelar (no se escribe nada).
juce::Result renderProjectToWav (const model::Project& project,
                                 const std::vector<SampleData>& samples,
                                 double lengthBeats,
                                 const juce::File& output,
                                 const OfflineRenderSettings& settings = {},
                                 const std::function<bool (double)>& progress = {});

} // namespace chimedes::audio
