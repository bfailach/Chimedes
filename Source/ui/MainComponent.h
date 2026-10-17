#pragma once

#include <memory>

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "audio/AudioEngine.h"
#include "engine/Schedule.h"
#include "model/Track.h"
#include "ui/GraphView.h"
#include "ui/Inspector.h"

namespace chimedes::ui
{

// Ventana principal: transporte arriba, lienzo en el centro y propiedades a la
// derecha. Es la dueña de la pista: aplica cada cambio, lo registra para deshacer,
// recalcula el horario y lo publica al audio.
class MainComponent : public juce::Component,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    class TrackChange;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;  // el historial cambió

    // Cambios a la pista
    void applyEdit (const EditCallbacks::Change& change);
    void beginGesture();
    void updateGesture (const EditCallbacks::Change& change);
    void endGesture();
    void setTrack (const model::Track& newTrack);  // usado por deshacer y rehacer
    void trackChanged();

    void rebuildSchedule();
    void togglePlayPause();
    void updateUndoButtons();

    // Audio
    audio::AudioEngine engine;
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer audioPlayer;

    // Datos
    model::Track track;
    std::shared_ptr<const engine::Schedule> schedule;
    juce::UndoManager undoManager;
    std::optional<model::Track> gestureStart;

    // Interfaz
    juce::TextButton playButton { "Reproducir" };
    juce::TextButton pauseButton { "Pausa" };
    juce::TextButton stopButton { "Detener" };
    juce::Label bpmLabel { {}, "BPM" };
    juce::Slider bpmSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::TextButton newButton { "Nuevo" };
    juce::TextButton undoButton { "Deshacer" };
    juce::TextButton redoButton { "Rehacer" };
    juce::Label statusLabel;
    GraphView graphView;
    Inspector inspector;

    juce::String audioWarning;
    int timerTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace chimedes::ui
