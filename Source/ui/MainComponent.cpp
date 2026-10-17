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
    constexpr int inspectorWidth = 270;

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
                    case Problem::MissingStart: return "Falta el nodo de inicio (clic derecho en un nodo)";
                    case Problem::MissingEnd:   return "Falta el nodo de fin (clic derecho en un nodo)";
                    case Problem::Unreachable:  return "Hay nodos inalcanzables desde el inicio (en naranja)";
                    case Problem::None:         break;
                }
                break;
        }

        return "La pista no se puede reproducir";
    }
}

// Una entrada de deshacer: la pista antes y después del cambio.
class MainComponent::TrackChange : public juce::UndoableAction
{
public:
    TrackChange (MainComponent& ownerToUse, model::Track beforeChange, model::Track afterChange)
        : owner (ownerToUse), before (std::move (beforeChange)), after (std::move (afterChange)) {}

    bool perform() override { owner.setTrack (after); return true; }
    bool undo() override    { owner.setTrack (before); return true; }

private:
    MainComponent& owner;
    model::Track before, after;
};

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

    newButton.onClick = [this]
    {
        // Pista vacía con las mismas opciones; se puede deshacer.
        applyEdit ([] (model::Track& t)
        {
            t.nodes.clear();
            t.edges.clear();
            t.startNode = model::noNode;
            t.endNode = model::noNode;
        });
    };
    undoButton.onClick = [this] { undoManager.undo(); };
    redoButton.onClick = [this] { undoManager.redo(); };

    statusLabel.setColour (juce::Label::textColourId, errorColour);

    // --- Edición -----------------------------------------------------------
    const EditCallbacks editCallbacks {
        [this] (const EditCallbacks::Change& c) { applyEdit (c); },
        [this] { beginGesture(); },
        [this] (const EditCallbacks::Change& c) { updateGesture (c); },
        [this] { endGesture(); }
    };

    graphView.callbacks = editCallbacks;
    inspector.callbacks = editCallbacks;
    graphView.onSelectionChanged = [this] (model::NodeId node) { inspector.refresh (track, node); };
    graphView.setTrack (&track);

    for (auto* c : std::initializer_list<juce::Component*> {
             &playButton, &pauseButton, &stopButton, &bpmLabel, &bpmSlider, &newButton,
             &undoButton, &redoButton, &statusLabel, &graphView, &inspector })
        addAndMakeVisible (c);

    // --- Audio -------------------------------------------------------------
    const auto audioError = deviceManager.initialiseWithDefaultDevices (0, 2);
    audioPlayer.setSource (&engine);
    deviceManager.addAudioCallback (&audioPlayer);

    // El transporte avanza con el audio: sin dispositivo, la reproducción no se mueve.
    if (audioError.isNotEmpty())
        audioWarning = "Sin audio: " + audioError;
    else if (deviceManager.getCurrentAudioDevice() == nullptr)
        audioWarning = text ("No se encontró ningún dispositivo de salida de audio");

    undoManager.addChangeListener (this);
    trackChanged();
    updateUndoButtons();

    setWantsKeyboardFocus (true);
    setSize (1400, 800);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    undoManager.removeChangeListener (this);
    deviceManager.removeAudioCallback (&audioPlayer);
    audioPlayer.setSource (nullptr);
}

// ---------------------------------------------------------------------------
// Cambios a la pista
// ---------------------------------------------------------------------------

void MainComponent::applyEdit (const EditCallbacks::Change& change)
{
    auto after = track;
    change (after);

    if (after == track)
        return;

    undoManager.beginNewTransaction();
    undoManager.perform (new TrackChange (*this, track, std::move (after)));
}

void MainComponent::beginGesture()
{
    gestureStart = track;
}

void MainComponent::updateGesture (const EditCallbacks::Change& change)
{
    if (! gestureStart.has_value())
        gestureStart = track;

    change (track);
    trackChanged();
}

void MainComponent::endGesture()
{
    if (! gestureStart.has_value())
        return;

    auto before = std::move (*gestureStart);
    gestureStart.reset();

    if (before == track)
        return;

    // El cambio ya está aplicado; la acción solo lo registra para deshacer.
    undoManager.beginNewTransaction();
    undoManager.perform (new TrackChange (*this, std::move (before), track));
}

void MainComponent::setTrack (const model::Track& newTrack)
{
    track = newTrack;
    trackChanged();
}

void MainComponent::trackChanged()
{
    rebuildSchedule();
    inspector.refresh (track, graphView.getSelectedNode());
    graphView.repaint();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateUndoButtons();
}

void MainComponent::rebuildSchedule()
{
    const auto result = engine::buildSchedule (track);

    schedule = result.ok() ? std::make_shared<const engine::Schedule> (*result.schedule) : nullptr;
    engine.publishSchedule (0, schedule);

    graphView.setSchedule (schedule);
    graphView.setUnreachable (result.validation.unreachable);

    const auto problem = track.nodes.empty() ? juce::String() : describe (result);
    statusLabel.setText (problem.isNotEmpty() ? problem : audioWarning, juce::dontSendNotification);
}

void MainComponent::updateUndoButtons()
{
    undoButton.setEnabled (undoManager.canUndo());
    redoButton.setEnabled (undoManager.canRedo());
}

// ---------------------------------------------------------------------------
// Transporte y teclado
// ---------------------------------------------------------------------------

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
    const auto command = juce::ModifierKeys::commandModifier;  // Ctrl en Windows y Linux, Cmd en macOS

    if (key == juce::KeyPress::spaceKey)
    {
        togglePlayPause();
        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        graphView.deleteSelection();
        return true;
    }

    if (key == juce::KeyPress ('z', command, 0))
    {
        undoManager.undo();
        return true;
    }

    if (key == juce::KeyPress ('z', command | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('y', command, 0))
    {
        undoManager.redo();
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Distribución
// ---------------------------------------------------------------------------

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
    place (bpmSlider, 160);
    bar.removeFromLeft (16);
    place (newButton, 64);
    place (undoButton, 80);
    place (redoButton, 72);
    bar.removeFromLeft (10);
    statusLabel.setBounds (bar);

    inspector.setBounds (area.removeFromRight (inspectorWidth));
    graphView.setBounds (area);
}

} // namespace chimedes::ui
