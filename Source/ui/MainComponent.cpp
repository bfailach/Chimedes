#include "MainComponent.h"

#include <cmath>

#include "engine/Scheduler.h"
#include "model/DemoProject.h"
#include "ui/Text.h"

namespace chimedes::ui
{

namespace
{
    constexpr int toolbarHeight = 48;

    const juce::Colour toolbarColour { 0xff1b1d23 };
    const juce::Colour errorColour { 0xffe0a84f };

    juce::String describe (const engine::ScheduleResult& result)
    {
        using Error = engine::ScheduleResult::Error;
        using Problem = engine::ValidationResult::Problem;

        switch (result.error)
        {
            case Error::None:            return {};
            case Error::EndBeforeStart:  return text ("El nodo de fin está a la izquierda del de inicio");
            case Error::TreeUnavailable: return "Prim solo funciona en grafos no dirigidos";
            case Error::Invalid:
                switch (result.validation.problem)
                {
                    case Problem::NoNodes:      return "La pista no tiene nodos";
                    case Problem::MissingStart: return "Falta el nodo de inicio";
                    case Problem::MissingEnd:   return "Falta el nodo de fin";
                    case Problem::Unreachable:  return "Hay nodos inalcanzables desde el inicio (en naranja)";
                    case Problem::None:         break;
                }
                break;
        }

        return "La pista no se puede reproducir";
    }
}

MainComponent::MainComponent()
    : track (model::makeDemoTrack())
{
    // --- Transporte --------------------------------------------------------
    playButton.onClick = [this] { engine.play(); };
    pauseButton.onClick = [this] { engine.pause(); };
    stopButton.onClick = [this] { engine.stop(); };

    bpmLabel.setJustificationType (juce::Justification::centredRight);
    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setValue (engine.getBpm(), juce::dontSendNotification);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 22);
    bpmSlider.onValueChange = [this] { engine.setBpm (bpmSlider.getValue()); };

    // --- Opciones de la pista ---------------------------------------------
    modeBox.addItemList ({ text ("Línea de tiempo"), "Algoritmo" }, 1);
    treeBox.addItemList ({ text ("Árbol: BFS"), text ("Árbol: DFS"), text ("Árbol: Dijkstra"), text ("Árbol: Prim") }, 1);
    traversalBox.addItemList ({ "Recorrido: BFS", "Recorrido: DFS" }, 1);
    siblingsBox.addItemList ({ "Hermanos: en secuencia", "Hermanos: a la vez" }, 1);

    modeBox.setSelectedItemIndex (0, juce::dontSendNotification);
    treeBox.setSelectedItemIndex (0, juce::dontSendNotification);
    traversalBox.setSelectedItemIndex (0, juce::dontSendNotification);
    siblingsBox.setSelectedItemIndex (0, juce::dontSendNotification);
    directedToggle.setToggleState (track.directed, juce::dontSendNotification);

    for (auto* box : { &modeBox, &treeBox, &traversalBox, &siblingsBox })
        box->onChange = [this] { rebuildSchedule(); };

    directedToggle.onClick = [this] { rebuildSchedule(); };

    statusLabel.setColour (juce::Label::textColourId, errorColour);

    for (auto* c : std::initializer_list<juce::Component*> {
             &playButton, &pauseButton, &stopButton, &bpmLabel, &bpmSlider, &modeBox, &treeBox,
             &traversalBox, &siblingsBox, &directedToggle, &statusLabel, &graphView })
        addAndMakeVisible (c);

    graphView.setTrack (&track);

    // --- Audio -------------------------------------------------------------
    const auto audioError = deviceManager.initialiseWithDefaultDevices (0, 2);
    audioPlayer.setSource (&engine);
    deviceManager.addAudioCallback (&audioPlayer);

    // El transporte avanza con el audio: sin dispositivo, la reproducción no se mueve.
    if (audioError.isNotEmpty())
        audioWarning = "Sin audio: " + audioError;
    else if (deviceManager.getCurrentAudioDevice() == nullptr)
        audioWarning = text ("No se encontró ningún dispositivo de salida de audio");

    rebuildSchedule();

    setWantsKeyboardFocus (true);
    setSize (1440, 760);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    deviceManager.removeAudioCallback (&audioPlayer);
    audioPlayer.setSource (nullptr);
}

void MainComponent::rebuildSchedule()
{
    track.mode = modeBox.getSelectedItemIndex() == 0 ? model::PlaybackMode::Timeline
                                                     : model::PlaybackMode::Algorithm;
    track.treeMethod = static_cast<model::TreeMethod> (treeBox.getSelectedItemIndex());
    track.traversal = static_cast<model::Traversal> (traversalBox.getSelectedItemIndex());
    track.siblings = static_cast<model::SiblingMode> (siblingsBox.getSelectedItemIndex());
    track.directed = directedToggle.getToggleState();

    const auto result = engine::buildSchedule (track);

    schedule = result.ok() ? std::make_shared<const engine::Schedule> (*result.schedule) : nullptr;
    engine.publishSchedule (0, schedule);

    graphView.setSchedule (schedule);
    graphView.setUnreachable (result.validation.unreachable);
    const auto problem = describe (result);
    statusLabel.setText (problem.isNotEmpty() ? problem : audioWarning, juce::dontSendNotification);

    updateControlsEnabled();
}

void MainComponent::updateControlsEnabled()
{
    const bool algorithm = track.mode == model::PlaybackMode::Algorithm;
    treeBox.setEnabled (algorithm);
    traversalBox.setEnabled (algorithm);
    siblingsBox.setEnabled (algorithm);
}

void MainComponent::togglePlayPause()
{
    if (engine.isPlaying())
        engine.pause();
    else
        engine.play();
}

void MainComponent::timerCallback()
{
    const auto position = engine.getPositionBeats();
    const auto cycle = schedule != nullptr ? schedule->cycleLengthBeats : 0.0;
    const auto beatInCycle = cycle > 0.0 ? std::fmod (position, cycle) : 0.0;

    graphView.setPlayhead (beatInCycle, engine.isPlaying());

    playButton.setToggleState (engine.isPlaying(), juce::dontSendNotification);

    // Libera los horarios viejos unas dos veces por segundo.
    if (++timerTicks % 15 == 0)
        engine.collectGarbage();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        togglePlayPause();
        return true;
    }

    return false;
}

void MainComponent::paint (juce::Graphics& g)
{
    g.setColour (toolbarColour);
    g.fillRect (getLocalBounds().removeFromTop (toolbarHeight));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    auto bar = area.removeFromTop (toolbarHeight).reduced (8, 10);

    const auto place = [&bar] (juce::Component& c, int width)
    {
        c.setBounds (bar.removeFromLeft (width));
        bar.removeFromLeft (6);
    };

    place (playButton, 92);
    place (pauseButton, 64);
    place (stopButton, 70);
    bar.removeFromLeft (10);
    place (bpmLabel, 36);
    place (bpmSlider, 150);
    bar.removeFromLeft (10);
    place (modeBox, 140);
    place (treeBox, 130);
    place (traversalBox, 130);
    place (siblingsBox, 170);
    place (directedToggle, 90);
    statusLabel.setBounds (bar);

    graphView.setBounds (area);
}

} // namespace chimedes::ui
