#include "audio/OfflineRender.h"

#include "audio/AudioEngine.h"
#include "engine/Scheduler.h"

namespace chimedes::audio
{

juce::Result renderProjectToWav (const model::Project& project,
                                 const std::vector<SampleData>& samples,
                                 double lengthBeats,
                                 const juce::File& output,
                                 const OfflineRenderSettings& settings,
                                 const std::function<bool (double)>& progress)
{
    constexpr int blockSize = 512;
    constexpr double maxSeconds = 20.0 * 60.0;

    if (lengthBeats <= 0.0 || project.bpm <= 0.0)
        return juce::Result::fail ("No track is playing.");

    const auto musicSeconds = lengthBeats * 60.0 / project.bpm;
    if (musicSeconds > maxSeconds)
        return juce::Result::fail ("The export would be longer than 20 minutes.");

    // Separate engine: the app's keeps playing (or not) without noticing.
    AudioEngine engine;
    engine.prepareToPlay (blockSize, settings.sampleRate);
    engine.setBpm (project.bpm);
    engine.setSamples (samples);

    for (std::size_t i = 0; i < project.tracks.size() && i < static_cast<std::size_t> (AudioEngine::maxTracks); ++i)
    {
        const auto& t = project.tracks[i];
        if (t.muted)
            continue;

        if (const auto result = engine::buildSchedule (t); result.ok())
            engine.publishSchedule (static_cast<int> (i), std::make_shared<const engine::Schedule> (*result.schedule));
    }

    engine.play();

    const auto musicSamples = static_cast<int> (std::llround (musicSeconds * settings.sampleRate));
    const auto totalSamples = musicSamples + static_cast<int> (settings.tailSeconds * settings.sampleRate);

    juce::AudioBuffer<float> rendered (2, totalSamples);
    rendered.clear();

    // Music until the exact end; then pause (notes die out with their tail).
    for (int start = 0; start < totalSamples;)
    {
        if (start == musicSamples)
            engine.pause();

        const auto limit = start < musicSamples ? musicSamples : totalSamples;
        const auto n = std::min (blockSize, limit - start);
        engine.getNextAudioBlock (juce::AudioSourceChannelInfo (&rendered, start, n));
        start += n;

        if (progress && ! progress (0.9 * start / totalSamples))
            return juce::Result::fail ("cancelled");
    }

    // No clipping: if anything goes over 0 dB, everything is turned down evenly.
    const auto peak = rendered.getMagnitude (0, totalSamples);
    if (peak > 0.98f)
        rendered.applyGain (0.98f / peak);

    output.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (output.createOutputStream());
    if (stream == nullptr)
        return juce::Result::fail ("Could not create the file.");

    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream,
                                       juce::AudioFormatWriterOptions{}
                                           .withSampleRate (settings.sampleRate)
                                           .withNumChannels (2)
                                           .withBitsPerSample (settings.bitsPerSample));
    if (writer == nullptr)
        return juce::Result::fail ("Could not write the WAV.");

    if (! writer->writeFromAudioSampleBuffer (rendered, 0, totalSamples))
        return juce::Result::fail ("Could not write the WAV (is the disk full?).");

    if (progress)
        progress (1.0);

    return juce::Result::ok();
}

} // namespace chimedes::audio
