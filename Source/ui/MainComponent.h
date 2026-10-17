#pragma once

#include <memory>

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "audio/AudioEngine.h"
#include "engine/Schedule.h"
#include "model/Track.h"
#include "ui/GraphView.h"

namespace chimedes::ui
{

// Ventana principal: barra de transporte arriba y el grafo debajo.
// Mientras no exista el editor, reproduce un grafo de demostración y deja
// cambiar el modo, el método de árbol, el recorrido y el modo de hermanos.
class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;

    // Lee los controles, recalcula el horario y lo publica al audio.
    void rebuildSchedule();
    void updateControlsEnabled();
    void togglePlayPause();

    // Audio
    audio::AudioEngine engine;
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer audioPlayer;

    // Datos
    model::Track track;
    std::shared_ptr<const engine::Schedule> schedule;

    // Interfaz
    juce::TextButton playButton { "Reproducir" };
    juce::TextButton pauseButton { "Pausa" };
    juce::TextButton stopButton { "Detener" };
    juce::Label bpmLabel { {}, "BPM" };
    juce::Slider bpmSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::ComboBox modeBox, treeBox, traversalBox, siblingsBox;
    juce::ToggleButton directedToggle { "Dirigido" };
    juce::Label statusLabel;
    GraphView graphView;

    juce::String audioWarning;
    int timerTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace chimedes::ui
