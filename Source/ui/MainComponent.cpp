#include "MainComponent.h"

#include <cmath>

#include "engine/Scheduler.h"
#include "model/DemoProject.h"
#include "model/Instruments.h"
#include "audio/OfflineRender.h"
#include "engine/Export.h"
#include "model/ProjectFile.h"
#include "model/Randomizer.h"
#include "ui/Text.h"
#include "ui/Theme.h"

namespace chimedes::ui
{

namespace
{
    constexpr int margin = 24;          // gap between the floating panels and the edge
    constexpr int inspectorWidth = 270;
    constexpr int barHeight = 52;
    constexpr int topBarHeight = 44;
    constexpr int topBarY = GraphView::rulerHeight + 10;   // below the time ruler

    juce::String describe (const engine::ScheduleResult& result)
    {
        using Error = engine::ScheduleResult::Error;
        using Problem = engine::ValidationResult::Problem;

        switch (result.error)
        {
            case Error::None:            return {};
            case Error::EndBeforeStart:  return "The end node is to the left of the start node";
            case Error::TreeUnavailable: return "Prim only works on undirected graphs";
            case Error::Invalid:
                switch (result.validation.problem)
                {
                    case Problem::NoNodes:      return "The track has no nodes";
                    case Problem::MissingStart: return "The start node is missing (right-click a node)";
                    case Problem::MissingEnd:   return "The end node is missing (right-click a node)";
                    case Problem::Unreachable:  return "Some nodes cannot be reached from the start (in orange)";
                    case Problem::None:         break;
                }
                break;
        }

        return "The track cannot be played";
    }
}

// An undo entry: the project before and after the change, and which track was
// active at each point (undo shows the affected track again).
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
        demo.name = "Track 1";
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

    // A free name such as "Track 3".
    std::string nextTrackName (const model::Project& p)
    {
        for (int n = static_cast<int> (p.tracks.size()) + 1;; ++n)
        {
            const auto name = "Track " + std::to_string (n);
            if (std::none_of (p.tracks.begin(), p.tracks.end(), [&name] (const model::Track& t) { return t.name == name; }))
                return name;
        }
    }
}

MainComponent::MainComponent()
    : project (makeDemoProject())
{
    // --- Transport ---------------------------------------------------------
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
    exportButton.onClick = [this] { showExportMenu(); };
    randomButton.setTooltip ("Random: graph, notes, rhythm, instruments or algorithm");
    randomButton.onClick = [this] { showRandomMenu(); };
    undoButton.onClick = [this] { undoManager.undo(); };
    redoButton.onClick = [this] { undoManager.redo(); };

    statusLabel.setColour (juce::Label::textColourId, theme::warning);
    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setJustificationType (juce::Justification::centred);

    // --- Editing -----------------------------------------------------------
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

    // --- Samples -----------------------------------------------------------
    formatManager.registerBasicFormats();
    inspector.onLoadSample = [this] (bool forNode) { loadSample (forNode); };

    // --- Tracks ------------------------------------------------------------
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

    // Preview notes from the panel's keyboard: they sound right away, even when not playing.
    inspector.onAudition = [this] (int pitch, int velocity, int instrument)
    {
        if (velocity > 0)
            engine.previewNoteOn (pitch, velocity, instrument);
        else
            engine.previewNoteOff (pitch, instrument);
    };

    // The canvas fills the whole window; the panels float on top.
    addAndMakeVisible (graphView);
    addAndMakeVisible (inspector);
    addAndMakeVisible (trackList);
    addAndMakeVisible (topBar);
    addAndMakeVisible (bottomBar);

    // Top bar: file actions and the project name.
    projectLabel.setColour (juce::Label::textColourId, theme::textPrimary);
    projectLabel.setFont (theme::sans (13.0f));
    projectLabel.setInterceptsMouseClicks (false, false);
    for (auto* c : std::initializer_list<juce::Component*> { &newButton, &openButton, &saveButton, &exportButton, &projectLabel })
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

    // The transport moves with the audio: without a device, playback does not move.
    if (audioError.isNotEmpty())
        audioWarning = "No audio: " + audioError;
    else if (deviceManager.getCurrentAudioDevice() == nullptr)
        audioWarning = "No audio output device was found";

    undoManager.addChangeListener (this);
    projectChanged();
    updateUndoButtons();
    markSaved();   // the demo graph does not count as an unsaved change

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
// Track changes
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

    // The change is already applied; the action only records it for undo.
    undoManager.beginNewTransaction();
    undoManager.perform (new ProjectChange (*this, std::move (before), active, project, active));
}

void MainComponent::setProject (const model::Project& newProject, std::size_t newActive)
{
    const auto trackChanged = newActive != active
                           || newActive >= project.tracks.size()
                           || newActive >= newProject.tracks.size()
                           || project.tracks[newActive].id != newProject.tracks[newActive].id;

    // Another track: the canvas selection belonged to the previous one. It is cleared
    // before replacing the project: the canvas still points at the current tracks and,
    // if the track vector changes size, those pointers stay invalid until
    // projectChanged() hands it the new ones.
    if (trackChanged)
        graphView.selectNode (model::noNode);

    project = newProject;
    if (project.tracks.empty())
        project.tracks.push_back ({});
    active = std::min (newActive, project.tracks.size() - 1);

    projectChanged();
}

void MainComponent::setActiveTrack (std::size_t index)
{
    if (index >= project.tracks.size() || index == active)
        return;

    // Switching tracks is not an edit: it does not go into the history.
    graphView.selectNode (model::noNode);
    active = index;
    projectChanged();
}

void MainComponent::projectChanged()
{
    syncSamples();
    rebuildSchedules();
    inspector.refresh (activeTrack(), graphView.getSelectedNode());
    trackList.refresh (project, active);
    graphView.repaint();
    updateTitle();
    resized();   // the track list changes height
}

void MainComponent::addTrack()
{
    if (project.tracks.size() >= TrackList::maxTracks)
        return;

    model::Track t;
    t.id = nextTrackId (project);
    t.name = nextTrackName (project);
    // Each new track gets a different instrument, so they can be told apart by ear.
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
    copy.name = (copy.name.empty() ? std::string ("Track") : copy.name) + " (copy)";

    applyProjectEdit ([index, copy] (model::Project& p)
    {
        p.tracks.insert (p.tracks.begin() + static_cast<std::ptrdiff_t> (index) + 1, copy);
    }, index + 1);
}

void MainComponent::deleteTrack (std::size_t index)
{
    if (project.tracks.size() <= 1 || index >= project.tracks.size())
        return;

    // It can be undone, so there is no need to ask.
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
    // Each track plays in its own engine slot, with its own cycle.
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

        // Warnings: the active track's first; if none, another track's (with its name).
        const auto issue = t.nodes.empty() || t.muted ? juce::String() : describe (result);
        if (issue.isNotEmpty() && (i == active || problem.isEmpty()))
            problem = i == active ? issue
                                  : juce::String::fromUTF8 (t.name.c_str()) + ": " + issue.substring (0, 1).toLowerCase() + issue.substring (1);
    }

    // Leftover slots (deleted tracks) are silenced.
    for (auto i = static_cast<int> (project.tracks.size()); i < audio::AudioEngine::maxTracks; ++i)
        engine.publishSchedule (i, nullptr);

    graphView.setTracks (std::move (views), active);

    const auto status = problem.isNotEmpty() ? problem : sampleWarning.isNotEmpty() ? sampleWarning : audioWarning;
    if (status != statusLabel.getText())
    {
        statusLabel.setText (status, juce::dontSendNotification);
        resized();   // the warning appears, disappears or changes length
    }
}

void MainComponent::updateUndoButtons()
{
    undoButton.setEnabled (undoManager.canUndo());
    redoButton.setEnabled (undoManager.canRedo());
}

// ---------------------------------------------------------------------------
// Transport and keyboard
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
    // If the app goes to the background with a key held, its "release" would never arrive.
    if (! juce::Process::isForegroundProcess())
        inspector.releasePianoKeys();

    graphView.setPlayhead (engine.getPositionBeats(), engine.isPlaying(), engine.getBpm());
    playButton.setToggleState (engine.isPlaying(), juce::dontSendNotification);

    // Frees old schedules about twice a second.
    if (++timerTicks % 15 == 0)
        engine.collectGarbage();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    const auto command = juce::ModifierKeys::commandModifier;  // Ctrl on Windows and Linux, Cmd on macOS

    // With a node selected, letters play the piano and Enter / Esc save or
    // discard the proposed note.
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
    // On letter press and release: so they sound while held and form chords.
    return inspector.handlePianoKeyStateChange();
}

void MainComponent::focusLost (FocusChangeType)
{
    // If the window loses focus with a key held, its "release" would never arrive.
    if (! hasKeyboardFocus (true))
        inspector.releasePianoKeys();
}

// ---------------------------------------------------------------------------
// Layout
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

    // Top bar, centred and as wide as its content: [new, open, save, export] name •
    {
        constexpr int pad = 8;
        const auto nameWidth = juce::jlimit (60, 320, juce::roundToInt (juce::GlyphArrangement::getStringWidth (
                                                          projectLabel.getFont(), projectLabel.getText())) + 8);
        const auto width = pad + 4 * 40 + 3 * 2 + 12 + nameWidth + 14;

        topBar.setBounds (juce::Rectangle<int> (width, topBarHeight)
                              .withCentre ({ getWidth() / 2, topBarY + topBarHeight / 2 }));

        auto bar = topBar.getLocalBounds().reduced (pad, 4);
        newButton.setBounds (bar.removeFromLeft (40));
        bar.removeFromLeft (2);
        openButton.setBounds (bar.removeFromLeft (40));
        bar.removeFromLeft (2);
        saveButton.setBounds (bar.removeFromLeft (40));
        bar.removeFromLeft (2);
        exportButton.setBounds (bar.removeFromLeft (40));
        bar.removeFromLeft (12);
        projectLabel.setBounds (bar);
    }

    // Bottom bar: just as wide as its content, centred at the bottom.
    // [BPM + slider]  [stop, play, pause]  [random]  [undo, redo]
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

        // Random; undo and redo together, a little further apart.
        randomButton.setBounds (bar.removeFromLeft (40));
        bar.removeFromLeft (14);
        undoButton.setBounds (bar.removeFromLeft (40));
        bar.removeFromLeft (2);
        redoButton.setBounds (bar.removeFromLeft (40));
    }

    // Floating warning right above the bar, as long as its text.
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

    // Track list at the top left, aligned with the top bar.
    {
        constexpr int listWidth = 230;
        const auto top = topBarY;
        const auto listHeight = std::min (trackList.getIdealHeight(), area.getBottom() - 6 - top);
        trackList.setBounds (area.getX(), top, listWidth, listHeight);
    }

    // The first time, the graph is framed in the space the panels leave free.
    if (! initialFitDone && getWidth() > 0 && getHeight() > 0)
    {
        initialFitDone = true;
        graphView.zoomToFit (freeCanvasArea());
    }

    // Floating right panel, vertically centred on the window, clear of the bar.
    area.removeFromBottom (6);
    const auto panelHeight = std::min (inspector.getIdealHeight(), area.getHeight());
    const auto centredTop = (getHeight() - panelHeight) / 2;
    const auto top = juce::jlimit (area.getY(), area.getBottom() - panelHeight, centredTop);
    inspector.setBounds (area.getRight() - inspectorWidth, top, inspectorWidth, panelHeight);
}

// ---------------------------------------------------------------------------
// Random
// ---------------------------------------------------------------------------

void MainComponent::showRandomMenu()
{
    enum Item { graph = 1, notes, rhythm, instruments, algorithm, everything, everythingAllTracks };

    const bool algorithmMode = activeTrack().mode == model::PlaybackMode::Algorithm;
    const bool empty = activeTrack().nodes.empty();

    juce::PopupMenu menu;
    menu.addSectionHeader ("Random, on the active track");
    menu.addItem (graph, "New graph");
    menu.addItem (notes, "Notes (same shape)", ! empty);
    menu.addItem (rhythm, "Rhythm: durations and velocities", ! empty);
    menu.addItem (instruments, "Instruments");
    menu.addItem (algorithm, algorithmMode ? "Algorithm: tree, traversal, siblings"
                                           : "Algorithm (algorithm mode only)", algorithmMode);
    menu.addItem (everything, "Everything");
    menu.addSeparator();
    menu.addItem (everythingAllTracks, "Everything, on every track");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&randomButton),
                        [safe = juce::Component::SafePointer<MainComponent> (this)] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        auto& self = *safe;
        const auto seed = static_cast<std::uint32_t> (juce::Random::getSystemRandom().nextInt());

        // Possible instruments: the built-ins and the samples that loaded.
        std::vector<model::InstrumentId> available;
        for (int id = model::instruments::first; id <= model::instruments::count; ++id)
            available.push_back (static_cast<model::InstrumentId> (id));
        for (const auto& smp : self.project.samples)
            if (self.decodedSamples.count (smp.path) > 0)
                available.push_back (smp.id);

        const bool newShape = result == graph || result == everything || result == everythingAllTracks;
        if (newShape)
            self.graphView.selectNode (model::noNode);

        switch (result)
        {
            case graph:       self.applyEdit ([seed] (model::Track& t) { model::randomizeGraph (t, seed); }); break;
            case notes:       self.applyEdit ([seed] (model::Track& t) { model::randomizeNotes (t, seed); }); break;
            case rhythm:      self.applyEdit ([seed] (model::Track& t) { model::randomizeRhythm (t, seed); }); break;
            case instruments: self.applyEdit ([seed, available] (model::Track& t) { model::randomizeInstruments (t, seed, available); }); break;
            case algorithm:   self.applyEdit ([seed] (model::Track& t) { model::randomizeAlgorithm (t, seed); }); break;
            case everything:  self.applyEdit ([seed, available] (model::Track& t) { model::randomizeEverything (t, seed, available); }); break;
            case everythingAllTracks:
                // A single undo step for every track; each with its own seed.
                self.applyProjectEdit ([seed, available] (model::Project& p)
                {
                    for (std::size_t i = 0; i < p.tracks.size(); ++i)
                        model::randomizeEverything (p.tracks[i], seed + static_cast<std::uint32_t> (i) * 7919u, available);
                });
                break;
            default: break;
        }

        // A new shape is framed in the part of the canvas the panels do not cover.
        if (newShape)
            self.graphView.zoomToFit (self.freeCanvasArea());
    });
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

namespace
{
    // WAV render in the background, with a progress bar and a cancel button.
    class WavExportJob : public juce::ThreadWithProgressWindow
    {
    public:
        WavExportJob (model::Project projectToRender, std::vector<audio::SampleData> samplesToUse,
                      double beats, juce::File target)
            : juce::ThreadWithProgressWindow (juce::String::fromUTF8 ("Exporting audio…"), true, true),
              project (std::move (projectToRender)), samples (std::move (samplesToUse)),
              lengthBeats (beats), file (std::move (target)) {}

        void run() override
        {
            result = audio::renderProjectToWav (project, samples, lengthBeats, file, {}, [this] (double p)
            {
                setProgress (p);
                return ! threadShouldExit();
            });
        }

        // On the UI thread, when finished or cancelled.
        void threadComplete (bool userPressedCancel) override
        {
            if (! userPressedCancel)
                juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                                  .withIconType (juce::MessageBoxIconType::NoIcon)
                                                  .withTitle (result.wasOk() ? juce::String ("Audio exported")
                                                                             : juce::String ("Export failed"))
                                                  .withMessage (result.wasOk() ? file.getFullPathName() : result.getErrorMessage())
                                                  .withButton ("OK"),
                                              nullptr);
            delete this;
        }

    private:
        model::Project project;
        std::vector<audio::SampleData> samples;
        double lengthBeats;
        juce::File file;
        juce::Result result { juce::Result::ok() };
    };

    juce::String formatSeconds (double seconds)
    {
        const auto total = static_cast<int> (std::ceil (seconds));
        return total < 60 ? juce::String (total) + " s"
                          : juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2) + " min";
    }
}

model::Project MainComponent::projectForExport() const
{
    auto p = project;
    p.bpm = engine.getBpm();
    return p;
}

void MainComponent::showExportMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, text ("Export audio (WAV)…"));
    menu.addItem (2, text ("Export MIDI…"));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportButton),
                        [safe = juce::Component::SafePointer<MainComponent> (this)] (int result)
    {
        if (safe != nullptr && result != 0)
            safe->exportProject (result == 1 ? ExportKind::Wav : ExportKind::Midi);
    });
}

void MainComponent::exportProject (ExportKind kind)
{
    constexpr double maxBeats = 4096.0;
    const auto p = projectForExport();
    const auto secondsFor = [&p] (double beats) { return beats * 60.0 / p.bpm; };

    if (engine::exportLengthBeats (p, 1, maxBeats) <= 0.0)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::NoIcon)
                                          .withTitle ("Nothing to export")
                                          .withMessage ("No track is playing: check that they have a start and an end, and that they are not muted.")
                                          .withButton ("OK"),
                                      nullptr);
        return;
    }

    // Possible lengths, with how long each one lasts at this BPM.
    struct Choice { juce::String label; double beats; };
    std::vector<Choice> choices;
    const auto full = engine::exportLengthBeats (p, 0, maxBeats);
    choices.push_back ({ "Full loop: until every track lines up again", full });
    for (const auto cycles : { 1, 2, 4, 8 })
        choices.push_back ({ cycles == 1 ? juce::String ("1 loop of the longest track")
                                         : juce::String (cycles) + " loops of the longest track",
                             engine::exportLengthBeats (p, cycles, maxBeats) });

    juce::StringArray labels;
    for (const auto& c : choices)
        labels.add (c.label + "  (" + formatSeconds (secondsFor (c.beats)) + ")");

    auto* dialog = new juce::AlertWindow (kind == ExportKind::Wav ? "Export audio (WAV)" : "Export MIDI",
                                          kind == ExportKind::Wav
                                              ? "Unmuted tracks play, with their instruments and samples, at " + juce::String (p.bpm, 0) + " BPM."
                                              : "One MIDI track per playing track, with its name, at " + juce::String (p.bpm, 0) + " BPM.",
                                          juce::MessageBoxIconType::NoIcon, this);
    dialog->addComboBox ("length", labels, "Length");
    auto choice = std::make_shared<int> (0);
    if (auto* box = dialog->getComboBoxComponent ("length"))
        box->onChange = [box, choice] { *choice = box->getSelectedItemIndex(); };
    dialog->addButton (text ("Choose file…"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<MainComponent> (this), kind, choices, choice, p] (int result)
    {
        if (safe == nullptr || result != 1)
            return;

        const auto beats = choices[static_cast<std::size_t> (std::clamp (*choice, 0, static_cast<int> (choices.size()) - 1))].beats;
        const auto extension = kind == ExportKind::Wav ? juce::String (".wav") : juce::String (".mid");
        const auto name = safe->projectDisplayName() + extension;

        safe->fileChooser = std::make_unique<juce::FileChooser> (kind == ExportKind::Wav ? "Export audio" : "Export MIDI",
                                                                 safe->defaultFolder().getChildFile (name), "*" + extension);

        safe->fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                            | juce::FileBrowserComponent::warnAboutOverwriting,
                                        [safe, kind, beats, extension, p] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (safe == nullptr || file == juce::File())
                return;
            if (! file.hasFileExtension (extension))
                file = file.withFileExtension (extension);
            file.getParentDirectory().createDirectory();

            if (kind == ExportKind::Wav)
            {
                (new WavExportJob (p, safe->sampleData, beats, file))->launchThread();
                return;
            }

            const auto bytes = engine::exportMidi (p, beats);
            const auto ok = file.replaceWithData (bytes.data(), bytes.size());
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                              .withIconType (juce::MessageBoxIconType::NoIcon)
                                              .withTitle (ok ? "MIDI exported" : "Export failed")
                                              .withMessage (ok ? file.getFullPathName()
                                                               : "Could not write " + file.getFullPathName())
                                              .withButton ("OK"),
                                          nullptr);
        });
    }), true);
}

// ---------------------------------------------------------------------------
// Sound samples
// ---------------------------------------------------------------------------

std::shared_ptr<const juce::AudioBuffer<float>> MainComponent::decodeSample (const juce::File& file, double& sampleRate,
                                                                             juce::String& error)
{
    constexpr double maxSeconds = 30.0;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
    {
        error = file.existsAsFile() ? "Unrecognised format (use WAV, AIFF, FLAC or OGG)."
                                    : "The file does not exist.";
        return nullptr;
    }

    // Up to 30 seconds, in stereo (a mono sample is copied to both channels).
    const auto length = static_cast<int> (std::min<juce::int64> (reader->lengthInSamples,
                                                                 static_cast<juce::int64> (maxSeconds * reader->sampleRate)));
    if (length < 2)
    {
        error = "The file is empty.";
        return nullptr;
    }

    auto buffer = std::make_shared<juce::AudioBuffer<float>> (2, length);
    reader->read (buffer.get(), 0, length, 0, true, true);
    if (reader->numChannels == 1)
        buffer->copyFrom (1, 0, *buffer, 0, 0, length);

    sampleRate = reader->sampleRate;
    return buffer;
}

void MainComponent::syncSamples()
{
    std::vector<std::pair<model::InstrumentId, juce::String>> choices;
    for (const auto& s : project.samples)
        choices.emplace_back (s.id, juce::String::fromUTF8 (s.name.c_str()));
    inspector.setSampleChoices (choices, project.samples.size() < static_cast<std::size_t> (model::instruments::maxSamples)
                                             || std::any_of (project.samples.begin(), project.samples.end(), [this] (const model::Sample& s)
                                                {
                                                    return std::none_of (project.tracks.begin(), project.tracks.end(), [&s] (const model::Track& t)
                                                    {
                                                        return t.instrument == s.id
                                                            || std::any_of (t.nodes.begin(), t.nodes.end(), [&s] (const model::Node& n) { return n.instrument == s.id; });
                                                    });
                                                }));

    if (project.samples == syncedSamples)
        return;

    std::vector<audio::SampleData> data;
    juce::StringArray missing;

    for (const auto& s : project.samples)
    {
        auto it = decodedSamples.find (s.path);
        if (it == decodedSamples.end())
        {
            DecodedSample decoded;
            juce::String error;
            decoded.buffer = decodeSample (juce::File (juce::String::fromUTF8 (s.path.c_str())), decoded.sampleRate, error);
            if (decoded.buffer == nullptr)
            {
                missing.add (juce::String::fromUTF8 (s.name.c_str()));
                continue;
            }
            it = decodedSamples.emplace (s.path, std::move (decoded)).first;
        }

        data.push_back ({ static_cast<int> (s.id), it->second.buffer, it->second.sampleRate, s.rootNote });
    }

    engine.setSamples (data);
    sampleData = data;
    syncedSamples = project.samples;

    // If any is missing, warn; that sample stays silent until it is back.
    sampleWarning = missing.isEmpty() ? juce::String()
                  : (missing.size() == 1 ? "Sample not found: " : "Samples not found: ")
                        + missing.joinIntoString (", ");
}

void MainComponent::loadSample (bool forNode)
{
    const auto node = graphView.getSelectedNode();
    const auto trackIndex = active;

    fileChooser = std::make_unique<juce::FileChooser> ("Load sound sample", defaultFolder(),
                                                       formatManager.getWildcardForAllFormats());

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, forNode, node, trackIndex] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file == juce::File())
            return;

        DecodedSample decoded;
        juce::String error;
        decoded.buffer = decodeSample (file, decoded.sampleRate, error);
        if (decoded.buffer == nullptr)
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                              .withIconType (juce::MessageBoxIconType::NoIcon)
                                              .withTitle ("Could not load " + file.getFileName())
                                              .withMessage (error)
                                              .withButton ("OK"),
                                          nullptr);
            return;
        }

        const auto path = file.getFullPathName().toStdString();
        decodedSamples[path] = std::move (decoded);

        applyProjectEdit ([&] (model::Project& p)
        {
            // A sample already loaded is reused; if there is no room, the ones no
            // track or node uses are removed.
            model::InstrumentId id = 0;
            for (const auto& s : p.samples)
                if (s.path == path)
                    id = s.id;

            if (id == 0 && model::freeSampleId (p) == 0)
            {
                const auto used = [&p] (model::InstrumentId sid)
                {
                    return std::any_of (p.tracks.begin(), p.tracks.end(), [sid] (const model::Track& t)
                    {
                        return t.instrument == sid
                            || std::any_of (t.nodes.begin(), t.nodes.end(), [sid] (const model::Node& n) { return n.instrument == sid; });
                    });
                };
                p.samples.erase (std::remove_if (p.samples.begin(), p.samples.end(),
                                                 [&used] (const model::Sample& s) { return ! used (s.id); }),
                                 p.samples.end());
            }

            if (id == 0)
            {
                id = model::freeSampleId (p);
                if (id == 0)
                    return;   // ten samples in use: the drop-down already says so

                model::Sample s;
                s.id = id;
                s.name = file.getFileNameWithoutExtension().toStdString();
                s.path = path;
                p.samples.push_back (s);
            }

            if (trackIndex >= p.tracks.size())
                return;

            auto& t = p.tracks[trackIndex];
            if (! forNode)
            {
                t.instrument = id;
            }
            else
            {
                for (auto& n : t.nodes)
                    if (n.id == node)
                        n.instrument = id;
            }
        });
    });
}

// ---------------------------------------------------------------------------
// Files
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
    // "Lyra Nova • — Chimedes": the dot marks unsaved changes.
    const auto name = projectDisplayName();
    const auto title = name + (hasUnsavedChanges() ? juce::String::fromUTF8 (" \xe2\x80\xa2") : juce::String())
                     + juce::String::fromUTF8 (" \xe2\x80\x94 Chimedes");

    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        if (window->getName() != title)
            window->setName (title);

    // The same name in the top bar; if its length changes, the bar adjusts.
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
    return currentFile == juce::File() ? juce::String ("Untitled") : currentFile.getFileNameWithoutExtension();
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

    const auto name = currentFile == juce::File() ? juce::String ("the untitled project")
                                                  : "\"" + currentFile.getFileName() + "\"";

    const auto options = juce::MessageBoxOptions()
                             .withIconType (juce::MessageBoxIconType::NoIcon)
                             .withTitle ("Save changes?")
                             .withMessage ("There are unsaved changes in " + name + ". If you don't save them, they will be lost.")
                             .withButton ("Save")
                             .withButton ("Don't save")
                             .withButton ("Cancel")
                             .withAssociatedComponent (this);

    // With three buttons, JUCE returns 1, 2 and 0 (the third also with Esc).
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
        // New document: a single empty track, with the active track's options.
        engine.stop();
        graphView.selectNode (model::noNode);

        auto empty = activeTrack();
        empty.id = 1;
        empty.name = "Track 1";
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
        fileChooser = std::make_unique<juce::FileChooser> ("Open project", defaultFolder(),
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
                                          .withTitle ("Could not open " + file.getFileName())
                                          .withMessage (message)
                                          .withButton ("OK"),
                                      nullptr);
    };

    if (! file.existsAsFile())
    {
        showError ("The file does not exist.");
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

    // Sample paths relative to the project folder → absolute.
    auto opened = loaded.project;
    for (auto& s : opened.samples)
    {
        const auto path = juce::String::fromUTF8 (s.path.c_str());
        if (! juce::File::isAbsolutePath (path))
            s.path = file.getParentDirectory().getChildFile (path).getFullPathName().toStdString();
    }

    currentFile = file;
    bpmSlider.setValue (loaded.project.bpm, juce::sendNotificationSync);
    setProject (opened, 0);
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
                               : defaultFolder().getChildFile ("Untitled" + juce::String (model::projectFileExtension));

    fileChooser = std::make_unique<juce::FileChooser> ("Save project", suggested,
                                                       juce::String ("*") + model::projectFileExtension);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, done = std::move (done)] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        if (file == juce::File())
        {
            if (done)
                done (false);   // cancelled
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

    // Samples inside the project folder are saved with a relative path:
    // so the whole folder can be moved or shared.
    const auto folder = file.getParentDirectory();
    for (auto& s : toSave.samples)
    {
        const juce::File sampleFile (juce::String::fromUTF8 (s.path.c_str()));
        if (sampleFile.isAChildOf (folder))
            s.path = sampleFile.getRelativePathFrom (folder).replaceCharacter ('\\', '/').toStdString();
    }

    // It is written to a temporary file and then replaced: if anything fails, the
    // previous file stays intact.
    juce::TemporaryFile temp (file);
    const auto json = juce::String::fromUTF8 (model::projectToJson (toSave).c_str());
    const auto ok = temp.getFile().replaceWithText (json, false, false, "\n") && temp.overwriteTargetFileWithTemporary();

    if (! ok)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::NoIcon)
                                          .withTitle ("Could not save")
                                          .withMessage ("Could not write " + file.getFullPathName()
                                                        + ". Check that the folder exists and that you have permission.")
                                          .withButton ("OK"),
                                      nullptr);
        return false;
    }

    currentFile = file;
    markSaved();
    return true;
}

} // namespace chimedes::ui
