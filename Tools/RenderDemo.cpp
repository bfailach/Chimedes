// Herramienta de desarrollo: renderiza el grafo de demostración a un WAV sin tarjeta
// de sonido, usando el mismo motor de audio que la aplicación. Sirve para comprobar
// el audio en máquinas sin altavoces y es la base de la futura exportación a WAV.
//
// Uso: ChimedesRender <salida.wav> [timeline|seq-bfs|seq-dfs|sim-bfs] [ciclos] [instrumento 1-6]

#include <iostream>

#include <juce_audio_formats/juce_audio_formats.h>

#include "audio/AudioEngine.h"
#include "engine/Scheduler.h"
#include "model/DemoProject.h"

using namespace chimedes;

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Uso: ChimedesRender <salida.wav> [timeline|seq-bfs|seq-dfs|sim-bfs] [ciclos] [instrumento 1-6]\n";
        return 1;
    }

    const juce::File output (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const juce::String mode = argc > 2 ? argv[2] : "timeline";
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
        std::cerr << "La pista no se puede reproducir.\n";
        return 1;
    }

    const auto schedule = std::make_shared<const engine::Schedule> (*result.schedule);
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr double bpm = 120.0;

    std::cout << "Modo " << mode << ": ciclo de " << schedule->cycleLengthBeats << " pulsos\n";
    std::cout << "Notas esperadas (segundos):";
    for (int c = 0; c < cycles; ++c)
        for (const auto& e : schedule->events)
            std::cout << " " << juce::String ((c * schedule->cycleLengthBeats + e.startBeat) * 60.0 / bpm, 3);
    std::cout << "\n";

    audio::AudioEngine engine;
    engine.prepareToPlay (blockSize, sampleRate);
    engine.setBpm (bpm);
    engine.publishSchedule (0, schedule);
    engine.play();

    // Los ciclos pedidos más un segundo para la cola de release.
    const auto totalSamples = static_cast<int> ((cycles * schedule->cycleLengthBeats * 60.0 / bpm + 1.0) * sampleRate);
    juce::AudioBuffer<float> rendered (2, totalSamples);
    rendered.clear();

    for (int start = 0; start < totalSamples; start += blockSize)
    {
        const auto n = std::min (blockSize, totalSamples - start);
        engine.getNextAudioBlock (juce::AudioSourceChannelInfo (&rendered, start, n));
    }

    // Detección sencilla de ataques: subidas bruscas de energía en ventanas de 5 ms.
    std::cout << "Ataques detectados (segundos):";
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

    std::cout << "\nPico: " << rendered.getMagnitude (0, 0, totalSamples)
              << "   RMS: " << rendered.getRMSLevel (0, 0, totalSamples)
              << "   ataques: " << onsets << "\n";

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
        std::cerr << "No se pudo escribir " << output.getFullPathName() << "\n";
        return 1;
    }

    writer->writeFromAudioSampleBuffer (rendered, 0, totalSamples);
    std::cout << "Escrito " << output.getFullPathName() << "\n";
    return 0;
}
