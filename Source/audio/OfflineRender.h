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
    double tailSeconds = 2.0;   // after the end, so notes die out on their own
};

// Renders the project to a WAV with the same engine that plays live (in a
// separate instance, without a sound card and faster than real time).
// Unmuted tracks play for `lengthBeats` beats at the project's BPM; after that,
// notes end with their tail. If the result goes over 0 dB, the whole thing is
// turned down so it does not clip.
//
// `progress` gets 0 to 1 and can return false to cancel (nothing is written).
juce::Result renderProjectToWav (const model::Project& project,
                                 const std::vector<SampleData>& samples,
                                 double lengthBeats,
                                 const juce::File& output,
                                 const OfflineRenderSettings& settings = {},
                                 const std::function<bool (double)>& progress = {});

} // namespace chimedes::audio
