// Development tool: renders the demo graph to a WAV without a sound card,
// using the same audio engine as the application. Useful to check the audio on
// machines without speakers.
//
// Usage: ChimedesRender <output.wav> [timeline|seq-bfs|seq-dfs|sim-bfs] [cycles] [instrument id] [bpm]
//        ChimedesRender <output.wav> <project.chimedes> [loops, 0 = full]
//
// The second form uses exactly the same export as the app (Export audio).

#include <iostream>

#include <juce_audio_formats/juce_audio_formats.h>

#include "audio/AudioEngine.h"
#include "audio/OfflineRender.h"
#include "engine/Export.h"
#include "engine/Scheduler.h"
#include "model/DemoProject.h"
#include "model/ProjectFile.h"

using namespace chimedes;

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: ChimedesRender <output.wav> [timeline|seq-bfs|seq-dfs|sim-bfs] [cycles] [instrument id] [bpm]\n";
        return 1;
    }

    const juce::File output (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const juce::String mode = argc > 2 ? argv[2] : "timeline";

    if (mode.endsWithIgnoreCase (".chimedes"))
    {
        const auto projectFile = juce::File::getCurrentWorkingDirectory().getChildFile (mode);
        const auto loaded = model::projectFromJson (projectFile.loadFileAsString().toStdString());
        if (! loaded.ok)
        {
            std::cerr << loaded.error << "\n";
            return 1;
        }

        // Samples: paths relative to the project folder, as in the app.
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::vector<audio::SampleData> samples;
        for (const auto& smp : loaded.project.samples)
        {
            const auto path = juce::String::fromUTF8 (smp.path.c_str());
            const auto file = juce::File::isAbsolutePath (path) ? juce::File (path) : projectFile.getParentDirectory().getChildFile (path);
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            if (reader == nullptr)
            {
                std::cerr << "Missing sample " << file.getFullPathName() << "\n";
                continue;
            }
            auto buffer = std::make_shared<juce::AudioBuffer<float>> (2, static_cast<int> (reader->lengthInSamples));
            reader->read (buffer.get(), 0, buffer->getNumSamples(), 0, true, true);
            if (reader->numChannels == 1)
                buffer->copyFrom (1, 0, *buffer, 0, 0, buffer->getNumSamples());
            samples.push_back ({ static_cast<int> (smp.id), buffer, reader->sampleRate, smp.rootNote });
        }

        const auto loops = argc > 3 ? juce::String (argv[3]).getIntValue() : 0;
        const auto beats = engine::exportLengthBeats (loaded.project, loops, 4096.0);
        std::cout << "Exporting " << beats << " beats at " << loaded.project.bpm << " BPM\n";

        const auto result = audio::renderProjectToWav (loaded.project, samples, beats, output);
        std::cout << (result.wasOk() ? "Written " + output.getFullPathName() : result.getErrorMessage()) << "\n";
        return result.wasOk() ? 0 : 1;
    }
    const int cycles = argc > 3 ? juce::String (argv[3]).getIntValue() : 2;

    auto track = model::makeDemoTrack();

    if (argc > 4)
        track.instrument = static_cast<model::InstrumentId> (juce::String (argv[4]).getIntValue());

    if (mode != "timeline")
    {
        track.mode = model::PlaybackMode::Algorithm;
        track.treeMethod = model::TreeMethod::BFS;
        track.traversal = mode.endsWith ("dfs") ? model::Traversal::DFS : model::Traversal::BFS;
        track.siblings = mode.startsWith ("sim") ? model::SiblingMode::Simultaneous : model::SiblingMode::Sequential;
    }

    const auto result = engine::buildSchedule (track);
    if (! result.ok())
    {
        std::cerr << "The track cannot be played.\n";
        return 1;
    }

    const auto schedule = std::make_shared<const engine::Schedule> (*result.schedule);
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    const double bpm = argc > 5 ? juce::String (argv[5]).getDoubleValue() : 120.0;

    std::cout << "Mode " << mode << ": cycle of " << schedule->cycleLengthBeats << " beats\n";
    std::cout << "Expected notes (seconds):";
    for (int c = 0; c < cycles; ++c)
        for (const auto& e : schedule->events)
            std::cout << " " << juce::String ((c * schedule->cycleLengthBeats + e.startBeat) * 60.0 / bpm, 3);
    std::cout << "\n";

    audio::AudioEngine engine;
    engine.prepareToPlay (blockSize, sampleRate);
    engine.setBpm (bpm);
    engine.publishSchedule (0, schedule);
    engine.play();

    // The requested cycles plus one second for the release tail.
    const auto totalSamples = static_cast<int> ((cycles * schedule->cycleLengthBeats * 60.0 / bpm + 1.0) * sampleRate);
    juce::AudioBuffer<float> rendered (2, totalSamples);
    rendered.clear();

    for (int start = 0; start < totalSamples; start += blockSize)
    {
        const auto n = std::min (blockSize, totalSamples - start);
        engine.getNextAudioBlock (juce::AudioSourceChannelInfo (&rendered, start, n));
    }

    // Simple onset detection: sudden rises in energy over 5 ms windows.
    std::cout << "Detected onsets (seconds):";
    constexpr int window = 240;
    float previous = 0.0f;
    double lastOnset = -1.0;
    int onsets = 0;

    for (int start = 0; start + window < totalSamples; start += window)
    {
        const auto rms = rendered.getRMSLevel (0, start, window);
        const auto time = start / sampleRate;

        if (rms > 0.02f && rms > previous * 1.4f && time - lastOnset > 0.1)
        {
            std::cout << " " << juce::String (time, 3);
            lastOnset = time;
            ++onsets;
        }
        previous = rms;
    }

    std::cout << "\nPeak: " << rendered.getMagnitude (0, 0, totalSamples)
              << "   RMS: " << rendered.getRMSLevel (0, 0, totalSamples)
              << "   onsets: " << onsets << "\n";

    juce::WavAudioFormat wav;
    output.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (output.createOutputStream());
    auto writer = wav.createWriterFor (stream,
                                       juce::AudioFormatWriterOptions{}
                                           .withSampleRate (sampleRate)
                                           .withNumChannels (2)
                                           .withBitsPerSample (24));
    if (writer == nullptr)
    {
        std::cerr << "Could not write " << output.getFullPathName() << "\n";
        return 1;
    }

    writer->writeFromAudioSampleBuffer (rendered, 0, totalSamples);
    std::cout << "Written " << output.getFullPathName() << "\n";
    return 0;
}
