#include "MainComponent.h"

#include <cmath>

#include "engine/Scheduler.h"
#include "model/DemoProject.h"
#include "model/Instruments.h"
#include "model/ProjectFile.h"
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
    constexpr int topBarHeight = 44;
    constexpr int topBarY = GraphView::rulerHeight + 10;   // debajo de la regla de tiempo

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

// Una entrada de deshacer: el proyecto antes y después del cambio, y qué pista
// estaba activa en cada momento (deshacer vuelve a mostrar la pista afectada).
class MainComponent::ProjectChange : public juce::UndoableAction
{
public:
    ProjectChange (MainComponent& ownerToUse, model::Project beforeChange, std::size_t activeBefore,
                   model::Project afterChange, std::size_t activeAfter)
        : owner (ownerToUse), before (std::move (beforeChange)), after (std::move (afterChange)),
          beforeActive (activeBefore), afterActive (activeAfter) {}

    bool perform() override { owner.setProject (after, afterActive); return true; }
    bool undo() override    { owner.setProject (before, beforeActive); return true; }

private:
    MainComponent& owner;
    model::Project before, after;
    std::size_t beforeActive, afterActive;
};

namespace
{
    model::Project makeDemoProject()
    {
        model::Project p;
        auto demo = model::makeDemoTrack();
        demo.name = "Pista 1";
        p.tracks = { demo };
        return p;
    }

    model::TrackId nextTrackId (const model::Project& p)
    {
        model::TrackId id = 1;
        for (const auto& t : p.tracks)
            id = std::max (id, t.id + 1);
        return id;
    }

    // Nombre libre del estilo "Pista 3".
    std::string nextTrackName (const model::Project& p)
    {
        for (int n = static_cast<int> (p.tracks.size()) + 1;; ++n)
        {
            const auto name = "Pista " + std::to_string (n);
            if (std::none_of (p.tracks.begin(), p.tracks.end(), [&name] (const model::Track& t) { return t.name == name; }))
                return name;
        }
    }
}

MainComponent::MainComponent()
    : project (makeDemoProject())
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
    bpmSlider.onValueChange = [this]
    {
        engine.setBpm (bpmSlider.getValue());
        updateTitle();
    };

    newButton.onClick = [this] { newProject(); };
    openButton.onClick = [this] { openProject(); };
    saveButton.onClick = [this] { saveProject(); };
    randomButton.setTooltip (text ("Grafo nuevo al azar: cantidad de nodos, forma, separación y duración"));
    randomButton.onClick = [this]
    {
        const auto seed = static_cast<std::uint32_t> (juce::Random::getSystemRandom().nextInt());
        graphView.selectNode (model::noNode);
        applyEdit ([seed] (model::Track& t) { model::randomizeGraph (t, seed); });

        // Encuadra el grafo nuevo en la parte del lienzo que no tapan los paneles.
        graphView.zoomToFit (freeCanvasArea());
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
    graphView.onSelectionChanged = [this] (model::NodeId node) { inspector.refresh (activeTrack(), node); };
    graphView.onActivateTrack = [this] (std::size_t index) { setActiveTrack (index); };

    // --- Pistas ------------------------------------------------------------
    trackList.onSelect = [this] (std::size_t i) { setActiveTrack (i); };
    trackList.onAdd = [this] { addTrack(); };
    trackList.onDuplicate = [this] (std::size_t i) { duplicateTrack (i); };
    trackList.onDelete = [this] (std::size_t i) { deleteTrack (i); };
    trackList.onToggleMute = [this] (std::size_t i)
    {
        applyProjectEdit ([i] (model::Project& p) { p.tracks[i].muted = ! p.tracks[i].muted; });
    };
    trackList.onRename = [this] (std::size_t i, juce::String name)
    {
        applyProjectEdit ([i, n = name.toStdString()] (model::Project& p) { p.tracks[i].name = n; });
    };

    // Notas de prueba del teclado del panel: suenan al momento, aunque no se reproduzca.
    inspector.onAudition = [this] (int pitch, int velocity, int instrument)
    {
        if (velocity > 0)
            engine.previewNoteOn (pitch, velocity, instrument);
        else
            engine.previewNoteOff (pitch, instrument);
    };

    // El lienzo ocupa toda la ventana; los paneles flotan encima.
    addAndMakeVisible (graphView);
    addAndMakeVisible (inspector);
    addAndMakeVisible (trackList);
    addAndMakeVisible (topBar);
    addAndMakeVisible (bottomBar);

    // Barra superior: acciones de archivo y el nombre del proyecto.
    projectLabel.setColour (juce::Label::textColourId, theme::textPrimary);
    projectLabel.setFont (theme::sans (13.0f));
    projectLabel.setInterceptsMouseClicks (false, false);
    for (auto* c : std::initializer_list<juce::Component*> { &newButton, &openButton, &saveButton, &projectLabel })
        topBar.addAndMakeVisible (c);
    addChildComponent (statusBar);
    statusBar.addAndMakeVisible (statusLabel);

    for (auto* c : std::initializer_list<juce::Component*> {
             &bpmLabel, &bpmSlider, &stopButton, &playButton, &pauseButton,
             &randomButton, &undoButton, &redoButton })
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
    projectChanged();
    updateUndoButtons();
    markSaved();   // el grafo de demostración no cuenta como cambio sin guardar

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
    auto after = project;
    change (after.tracks[active]);

    if (after == project)
        return;

    undoManager.beginNewTransaction();
    undoManager.perform (new ProjectChange (*this, project, active, std::move (after), active));
}

void MainComponent::applyProjectEdit (const std::function<void (model::Project&)>& change, std::optional<std::size_t> newActive)
{
    auto after = project;
    change (after);

    const auto afterActive = std::min (newActive.value_or (active), after.tracks.size() - 1);
    if (after == project && afterActive == active)
        return;

    undoManager.beginNewTransaction();
    undoManager.perform (new ProjectChange (*this, project, active, std::move (after), afterActive));
}

void MainComponent::beginGesture()
{
    gestureStart = project;
}

void MainComponent::updateGesture (const EditCallbacks::Change& change)
{
    if (! gestureStart.has_value())
        gestureStart = project;

    change (activeTrack());
    projectChanged();
}

void MainComponent::endGesture()
{
    if (! gestureStart.has_value())
        return;

    auto before = std::move (*gestureStart);
    gestureStart.reset();

    if (before == project)
        return;

    // El cambio ya está aplicado; la acción solo lo registra para deshacer.
    undoManager.beginNewTransaction();
    undoManager.perform (new ProjectChange (*this, std::move (before), active, project, active));
}

void MainComponent::setProject (const model::Project& newProject, std::size_t newActive)
{
    const auto trackChanged = newActive != active
                           || newActive >= project.tracks.size()
                           || newActive >= newProject.tracks.size()
                           || project.tracks[newActive].id != newProject.tracks[newActive].id;

    project = newProject;
    if (project.tracks.empty())
        project.tracks.push_back ({});
    active = std::min (newActive, project.tracks.size() - 1);

    // Otra pista: la selección del lienzo era de la anterior.
    if (trackChanged)
        graphView.selectNode (model::noNode);

    projectChanged();
}

void MainComponent::setActiveTrack (std::size_t index)
{
    if (index >= project.tracks.size() || index == active)
        return;

    // Cambiar de pista no es una edición: no entra en el historial.
    graphView.selectNode (model::noNode);
    active = index;
    projectChanged();
}

void MainComponent::projectChanged()
{
    rebuildSchedules();
    inspector.refresh (activeTrack(), graphView.getSelectedNode());
    trackList.refresh (project, active);
    graphView.repaint();
    updateTitle();
    resized();   // la lista de pistas cambia de alto
}

void MainComponent::addTrack()
{
    if (project.tracks.size() >= TrackList::maxTracks)
        return;

    model::Track t;
    t.id = nextTrackId (project);
    t.name = nextTrackName (project);
    // Cada pista nueva con otro instrumento, para distinguirlas al oído.
    t.instrument = static_cast<model::InstrumentId> (static_cast<int> (project.tracks.size()) % model::instruments::count + 1);
    t.mode = activeTrack().mode;
    t.directed = activeTrack().directed;

    const auto index = project.tracks.size();
    applyProjectEdit ([t] (model::Project& p) { p.tracks.push_back (t); }, index);
}

void MainComponent::duplicateTrack (std::size_t index)
{
    if (project.tracks.size() >= TrackList::maxTracks || index >= project.tracks.size())
        return;

    auto copy = project.tracks[index];
    copy.id = nextTrackId (project);
    copy.name = (copy.name.empty() ? std::string ("Pista") : copy.name) + " (copia)";

    applyProjectEdit ([index, copy] (model::Project& p)
    {
        p.tracks.insert (p.tracks.begin() + static_cast<std::ptrdiff_t> (index) + 1, copy);
    }, index + 1);
}

void MainComponent::deleteTrack (std::size_t index)
{
    if (project.tracks.size() <= 1 || index >= project.tracks.size())
        return;

    // Se puede deshacer, así que no hace falta preguntar.
    const auto newActive = index < active ? active - 1 : std::min (active, project.tracks.size() - 2);
    applyProjectEdit ([index] (model::Project& p)
    {
        p.tracks.erase (p.tracks.begin() + static_cast<std::ptrdiff_t> (index));
    }, newActive);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateUndoButtons();
}

void MainComponent::rebuildSchedules()
{
    // Cada pista suena en su propia ranura del motor, con su propio ciclo.
    schedules.assign (project.tracks.size(), nullptr);
    std::vector<GraphView::TrackView> views;
    juce::String problem;

    for (std::size_t i = 0; i < project.tracks.size(); ++i)
    {
        const auto& t = project.tracks[i];
        const auto result = engine::buildSchedule (t);

        if (result.ok())
            schedules[i] = std::make_shared<const engine::Schedule> (*result.schedule);

        engine.publishSchedule (static_cast<int> (i), t.muted ? nullptr : schedules[i]);
        views.push_back ({ &t, t.muted ? nullptr : schedules[i], result.validation.unreachable });

        // Avisos: los de la pista activa primero; si no hay, el de otra pista (con su nombre).
        const auto issue = t.nodes.empty() || t.muted ? juce::String() : describe (result);
        if (issue.isNotEmpty() && (i == active || problem.isEmpty()))
            problem = i == active ? issue
                                  : juce::String::fromUTF8 (t.name.c_str()) + ": " + issue.substring (0, 1).toLowerCase() + issue.substring (1);
    }

    // Las ranuras que sobran (pistas borradas) se silencian.
    for (auto i = static_cast<int> (project.tracks.size()); i < audio::AudioEngine::maxTracks; ++i)
        engine.publishSchedule (i, nullptr);

    graphView.setTracks (std::move (views), active);

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

    graphView.setPlayhead (engine.getPositionBeats(), engine.isPlaying(), engine.getBpm());
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

    if (key == juce::KeyPress ('s', command, 0))                                    { saveProject();   return true; }
    if (key == juce::KeyPress ('s', command | juce::ModifierKeys::shiftModifier, 0)) { saveProjectAs(); return true; }
    if (key == juce::KeyPress ('o', command, 0))                                    { openProject();   return true; }
    if (key == juce::KeyPress ('n', command, 0))                                    { newProject();    return true; }

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

    // Barra superior centrada, del ancho de su contenido: [nuevo, abrir, guardar] nombre •
    {
        constexpr int pad = 8;
        const auto nameWidth = juce::jlimit (60, 320, juce::roundToInt (juce::GlyphArrangement::getStringWidth (
                                                          projectLabel.getFont(), projectLabel.getText())) + 8);
        const auto width = pad + 3 * 40 + 2 * 2 + 12 + nameWidth + 14;

        topBar.setBounds (juce::Rectangle<int> (width, topBarHeight)
                              .withCentre ({ getWidth() / 2, topBarY + topBarHeight / 2 }));

        auto bar = topBar.getLocalBounds().reduced (pad, 4);
        newButton.setBounds (bar.removeFromLeft (40));
        bar.removeFromLeft (2);
        openButton.setBounds (bar.removeFromLeft (40));
        bar.removeFromLeft (2);
        saveButton.setBounds (bar.removeFromLeft (40));
        bar.removeFromLeft (12);
        projectLabel.setBounds (bar);
    }

    // Barra inferior: mide lo justo para su contenido y queda centrada abajo.
    // [BPM + control]  [detener, reproducir, pausa]  [aleatorio]  [deshacer, rehacer]
    constexpr int pad = 12;
    constexpr int groupGap = 20;
    constexpr int bpmWidth = 38 + 170;
    constexpr int transportWidth = 3 * 44;
    constexpr int actionsWidth = 3 * 40 + 14 + 2;
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

        // Aleatorio; deshacer y rehacer juntos, un poco más separados.
        randomButton.setBounds (bar.removeFromLeft (40));
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

    // Lista de pistas arriba a la izquierda, alineada con la barra superior.
    {
        constexpr int listWidth = 230;
        const auto top = topBarY;
        const auto listHeight = std::min (trackList.getIdealHeight(), area.getBottom() - 6 - top);
        trackList.setBounds (area.getX(), top, listWidth, listHeight);
    }

    // La primera vez, el grafo se encuadra en el espacio que dejan libre los paneles.
    if (! initialFitDone && getWidth() > 0 && getHeight() > 0)
    {
        initialFitDone = true;
        graphView.zoomToFit (freeCanvasArea());
    }

    // Panel derecho flotante, centrado en vertical respecto a la ventana, sin pisar la barra.
    area.removeFromBottom (6);
    const auto panelHeight = std::min (inspector.getIdealHeight(), area.getHeight());
    const auto centredTop = (getHeight() - panelHeight) / 2;
    const auto top = juce::jlimit (area.getY(), area.getBottom() - panelHeight, centredTop);
    inspector.setBounds (area.getRight() - inspectorWidth, top, inspectorWidth, panelHeight);
}

// ---------------------------------------------------------------------------
// Archivos
// ---------------------------------------------------------------------------

bool MainComponent::hasUnsavedChanges() const
{
    return project != savedProject || std::abs (engine.getBpm() - savedBpm) > 1e-9;
}

void MainComponent::markSaved()
{
    savedProject = project;
    savedBpm = engine.getBpm();
    updateTitle();
}

void MainComponent::updateTitle()
{
    // "Lyra Nova • — Chimedes": el punto marca cambios sin guardar.
    const auto name = projectDisplayName();
    const auto title = name + (hasUnsavedChanges() ? juce::String::fromUTF8 (" \xe2\x80\xa2") : juce::String())
                     + juce::String::fromUTF8 (" \xe2\x80\x94 Chimedes");

    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        if (window->getName() != title)
            window->setName (title);

    // El mismo nombre en la barra superior; si cambia de largo, la barra se ajusta.
    const auto label = projectDisplayName() + (hasUnsavedChanges() ? juce::String::fromUTF8 ("  \xe2\x80\xa2") : juce::String());
    if (projectLabel.getText() != label)
    {
        projectLabel.setText (label, juce::dontSendNotification);
        projectLabel.setTooltip (currentFile != juce::File() ? currentFile.getFullPathName() : juce::String());
        resized();
    }
}

juce::String MainComponent::projectDisplayName() const
{
    return currentFile == juce::File() ? text ("Sin título") : currentFile.getFileNameWithoutExtension();
}

juce::Rectangle<int> MainComponent::freeCanvasArea() const
{
    return getLocalBounds().reduced (margin)
               .withTrimmedTop (topBarY + topBarHeight + 8 - margin)
               .withTrimmedRight (inspectorWidth + 16)
               .withTrimmedLeft (230 + 16)
               .withTrimmedBottom (barHeight + 16);
}

void MainComponent::parentHierarchyChanged()
{
    updateTitle();
}

juce::File MainComponent::defaultFolder() const
{
    return currentFile != juce::File() ? currentFile.getParentDirectory()
                                       : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
}

void MainComponent::confirmDiscardChanges (std::function<void()> proceed)
{
    if (! hasUnsavedChanges())
    {
        proceed();
        return;
    }

    const auto name = currentFile == juce::File() ? text ("el proyecto sin título")
                                                  : "\"" + currentFile.getFileName() + "\"";

    const auto options = juce::MessageBoxOptions()
                             .withIconType (juce::MessageBoxIconType::NoIcon)
                             .withTitle (text ("¿Guardar los cambios?"))
                             .withMessage (text ("Hay cambios sin guardar en ") + name + text (". Si no los guardas, se perderán."))
                             .withButton ("Guardar")
                             .withButton ("No guardar")
                             .withButton ("Cancelar")
                             .withAssociatedComponent (this);

    // Con tres botones, JUCE devuelve 1, 2 y 0 (el tercero también con Esc).
    juce::AlertWindow::showAsync (options, [safeThis = juce::Component::SafePointer<MainComponent> (this),
                                            proceed = std::move (proceed)] (int result)
    {
        if (safeThis == nullptr)
            return;

        if (result == 1)
            safeThis->saveProject ([proceed] (bool saved) { if (saved) proceed(); });
        else if (result == 2)
            proceed();
    });
}

void MainComponent::newProject()
{
    confirmDiscardChanges ([this]
    {
        // Documento nuevo: una sola pista vacía, con las opciones de la pista activa.
        engine.stop();
        graphView.selectNode (model::noNode);

        auto empty = activeTrack();
        empty.id = 1;
        empty.name = "Pista 1";
        empty.nodes.clear();
        empty.edges.clear();
        empty.startNode = model::noNode;
        empty.endNode = model::noNode;
        empty.muted = false;

        model::Project fresh;
        fresh.tracks = { empty };

        undoManager.clearUndoHistory();
        currentFile = juce::File();
        setProject (fresh, 0);
        markSaved();
    });
}

void MainComponent::openProject()
{
    confirmDiscardChanges ([this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (text ("Abrir proyecto"), defaultFolder(),
                                                           juce::String ("*") + model::projectFileExtension);

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file != juce::File())
                openFile (file);
        });
    });
}

void MainComponent::openFile (const juce::File& file)
{
    const auto showError = [file] (const juce::String& message)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::NoIcon)
                                          .withTitle (text ("No se pudo abrir ") + file.getFileName())
                                          .withMessage (message)
                                          .withButton ("Aceptar"),
                                      nullptr);
    };

    if (! file.existsAsFile())
    {
        showError (text ("El archivo no existe."));
        return;
    }

    const auto loaded = model::projectFromJson (file.loadFileAsString().toStdString());
    if (! loaded.ok)
    {
        showError (juce::String::fromUTF8 (loaded.error.c_str()));
        return;
    }

    engine.stop();
    graphView.selectNode (model::noNode);
    undoManager.clearUndoHistory();

    currentFile = file;
    bpmSlider.setValue (loaded.project.bpm, juce::sendNotificationSync);
    setProject (loaded.project, 0);
    markSaved();

    graphView.zoomToFit (freeCanvasArea());
}

void MainComponent::saveProject (std::function<void (bool)> done)
{
    if (currentFile == juce::File())
    {
        saveProjectAs (std::move (done));
        return;
    }

    const auto ok = writeFile (currentFile);
    if (done)
        done (ok);
}

void MainComponent::saveProjectAs (std::function<void (bool)> done)
{
    const auto suggested = currentFile != juce::File()
                               ? currentFile
                               : defaultFolder().getChildFile (text ("Sin título") + model::projectFileExtension);

    fileChooser = std::make_unique<juce::FileChooser> (text ("Guardar proyecto"), suggested,
                                                       juce::String ("*") + model::projectFileExtension);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, done = std::move (done)] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        if (file == juce::File())
        {
            if (done)
                done (false);   // cancelado
            return;
        }

        if (! file.hasFileExtension (model::projectFileExtension))
            file = file.withFileExtension (model::projectFileExtension);

        const auto ok = writeFile (file);
        if (ok)
            currentFile = file;

        updateTitle();
        if (done)
            done (ok);
    });
}

bool MainComponent::writeFile (const juce::File& file)
{
    auto toSave = project;
    toSave.bpm = engine.getBpm();

    // Se escribe en un temporal y luego se reemplaza: si algo falla, el archivo
    // anterior queda intacto.
    juce::TemporaryFile temp (file);
    const auto json = juce::String::fromUTF8 (model::projectToJson (toSave).c_str());
    const auto ok = temp.getFile().replaceWithText (json, false, false, "\n") && temp.overwriteTargetFileWithTemporary();

    if (! ok)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::NoIcon)
                                          .withTitle (text ("No se pudo guardar"))
                                          .withMessage (text ("No se pudo escribir ") + file.getFullPathName()
                                                        + text (". Revisa que la carpeta exista y que tengas permiso."))
                                          .withButton ("Aceptar"),
                                      nullptr);
        return false;
    }

    currentFile = file;
    markSaved();
    return true;
}

} // namespace chimedes::ui
