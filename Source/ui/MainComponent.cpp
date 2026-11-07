#include "MainComponent.h"

#include <cmath>

#include "engine/Scheduler.h"
#include "model/DemoProject.h"
#include "model/Randomizer.h"
#include "ui/Text.h"
#include "ui/Theme.h"

namespace chimedes::ui
{

namespace
{
    constexpr int margin = 24;          // separación de los paneles flotantes al borde
    constexpr int inspectorWidth = 270;
    constexpr int barHeight = 52;

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

    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setValue (engine.getBpm(), juce::dontSendNotification);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 24);
    bpmSlider.onValueChange = [this] { engine.setBpm (bpmSlider.getValue()); };

    newButton.setTooltip (text ("Nuevo: lienzo vacío (se puede deshacer)"));
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
    randomButton.setTooltip (text ("Grafo nuevo al azar: cantidad de nodos, forma, separación y duración"));
    randomButton.onClick = [this]
    {
        const auto seed = static_cast<std::uint32_t> (juce::Random::getSystemRandom().nextInt());
        graphView.selectNode (model::noNode);
        applyEdit ([seed] (model::Track& t) { model::randomizeGraph (t, seed); });

        // Encuadra el grafo nuevo en la parte del lienzo que no tapan los paneles.
        graphView.zoomToFit (getLocalBounds().reduced (margin)
                                 .withTrimmedRight (inspectorWidth + 16)
                                 .withTrimmedBottom (barHeight + 16));
    };
    undoButton.onClick = [this] { undoManager.undo(); };
    redoButton.onClick = [this] { undoManager.redo(); };

    statusLabel.setColour (juce::Label::textColourId, theme::warning);
    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setJustificationType (juce::Justification::centred);

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

    // Notas de prueba del teclado del panel: suenan al momento, aunque no se reproduzca.
    inspector.onAudition = [this] (int pitch, int velocity, int instrument)
    {
        if (velocity > 0)
            engine.previewNoteOn (pitch, velocity, instrument);
        else
            engine.previewNoteOff (pitch, instrument);
    };
    graphView.setTrack (&track);

    // El lienzo ocupa toda la ventana; los paneles flotan encima.
    addAndMakeVisible (graphView);
    addAndMakeVisible (inspector);
    addAndMakeVisible (bottomBar);
    addChildComponent (statusBar);
    statusBar.addAndMakeVisible (statusLabel);

    for (auto* c : std::initializer_list<juce::Component*> {
             &bpmLabel, &bpmSlider, &stopButton, &playButton, &pauseButton,
             &randomButton, &newButton, &undoButton, &redoButton })
        bottomBar.addAndMakeVisible (c);

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
    setSize (1440, 900);
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
    const auto status = problem.isNotEmpty() ? problem : audioWarning;
    if (status != statusLabel.getText())
    {
        statusLabel.setText (status, juce::dontSendNotification);
        resized();   // el aviso aparece, desaparece o cambia de largo
    }
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
    // Si la app pasa a segundo plano con una tecla sostenida, nunca llegaría su "soltar".
    if (! juce::Process::isForegroundProcess())
        inspector.releasePianoKeys();

    const auto position = engine.getPositionBeats();
    const auto cycle = schedule != nullptr ? schedule->cycleLengthBeats : 0.0;
    const auto beatInCycle = cycle > 0.0 ? std::fmod (position, cycle) : 0.0;

    graphView.setPlayhead (beatInCycle, engine.isPlaying(), engine.getBpm());
    playButton.setToggleState (engine.isPlaying(), juce::dontSendNotification);

    // Libera los horarios viejos unas dos veces por segundo.
    if (++timerTicks % 15 == 0)
        engine.collectGarbage();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    const auto command = juce::ModifierKeys::commandModifier;  // Ctrl en Windows y Linux, Cmd en macOS

    // Con un nodo seleccionado, las letras tocan el piano y Enter / Esc guardan o
    // descartan la nota propuesta.
    if (inspector.handlePianoKeyPress (key))
        return true;

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

bool MainComponent::keyStateChanged (bool)
{
    // Al bajar y al soltar letras: así suenan mientras se sostienen y forman acordes.
    return inspector.handlePianoKeyStateChange();
}

void MainComponent::focusLost (FocusChangeType)
{
    // Si la ventana pierde el foco con una tecla sostenida, nunca llegaría su "soltar".
    if (! hasKeyboardFocus (true))
        inspector.releasePianoKeys();
}

// ---------------------------------------------------------------------------
// Distribución
// ---------------------------------------------------------------------------

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    graphView.setBounds (area);

    area.reduce (margin, margin);

    // Barra inferior: mide lo justo para su contenido y queda centrada abajo.
    // [BPM + control]  [detener, reproducir, pausa]  [aleatorio, nuevo]  [deshacer, rehacer]
    constexpr int pad = 12;
    constexpr int groupGap = 20;
    constexpr int bpmWidth = 38 + 170;
    constexpr int transportWidth = 3 * 44;
    constexpr int actionsWidth = 4 * 40 + 2 + 14 + 2;
    constexpr int barWidth = pad + bpmWidth + groupGap + transportWidth + groupGap + actionsWidth + pad;

    auto barArea = area.removeFromBottom (barHeight);
    bottomBar.setBounds (barArea.withSizeKeepingCentre (std::min (barWidth, barArea.getWidth()), barHeight));
    {
        auto bar = bottomBar.getLocalBounds().reduced (pad, 10);

        bpmLabel.setBounds (bar.removeFromLeft (38));
        bpmSlider.setBounds (bar.removeFromLeft (170));
        bar.removeFromLeft (groupGap);

        stopButton.setBounds (bar.removeFromLeft (44));
        playButton.setBounds (bar.removeFromLeft (44));
        pauseButton.setBounds (bar.removeFromLeft (44));
        bar.removeFromLeft (groupGap);

        // Aleatorio y nuevo juntos; deshacer y rehacer juntos, un poco más separados.
        randomButton.setBounds (bar.removeFromLeft (40));
        bar.removeFromLeft (2);
        newButton.setBounds (bar.removeFromLeft (40));
        bar.removeFromLeft (14);
        undoButton.setBounds (bar.removeFromLeft (40));
        bar.removeFromLeft (2);
        redoButton.setBounds (bar.removeFromLeft (40));
    }

    // Aviso flotante justo encima de la barra, del largo de su texto.
    area.removeFromBottom (10);
    const auto status = statusLabel.getText();
    statusBar.setVisible (status.isNotEmpty());
    if (status.isNotEmpty())
    {
        const auto textWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (statusLabel.getFont(), status));
        const auto width = std::min (textWidth + 36, area.getWidth());
        statusBar.setBounds (juce::Rectangle<int> (width, 30).withCentre ({ bottomBar.getBounds().getCentreX(), area.getBottom() - 15 }));
        statusLabel.setBounds (statusBar.getLocalBounds().reduced (12, 0));
        area.removeFromBottom (30);
    }

    // Panel derecho flotante, centrado en vertical respecto a la ventana, sin pisar la barra.
    area.removeFromBottom (6);
    const auto panelHeight = std::min (inspector.getIdealHeight(), area.getHeight());
    const auto centredTop = (getHeight() - panelHeight) / 2;
    const auto top = juce::jlimit (area.getY(), area.getBottom() - panelHeight, centredTop);
    inspector.setBounds (area.getRight() - inspectorWidth, top, inspectorWidth, panelHeight);
}

} // namespace chimedes::ui
