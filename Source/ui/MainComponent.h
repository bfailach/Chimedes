#pragma once

#include <functional>
#include <map>
#include <memory>

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "audio/AudioEngine.h"
#include "engine/Schedule.h"
#include "model/Track.h"
#include "ui/GraphView.h"
#include "ui/IconButton.h"
#include "ui/Inspector.h"
#include "ui/TrackList.h"
#include "ui/TransportButton.h"

namespace chimedes::ui
{

// Main window: canvas in the middle with floating panels for tracks, file
// actions, properties and transport. It owns the project: it applies every change,
// records it for undo, recomputes the schedules and publishes them to audio.
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
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType cause) override;
    void parentHierarchyChanged() override;

    // --- Files -----------------------------------------------------------------
    // If there are unsaved changes, asks (Save / Don't save / Cancel) and only
    // calls `proceed` if it is fine to go on. The app also uses it when closing.
    void confirmDiscardChanges (std::function<void()> proceed);

    // Opens a project (also from the command line, when double-clicking a file).
    void openFile (const juce::File& file);

private:
    class ProjectChange;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;  // the history changed

    // Changes to the active track (canvas and right panel), with undo.
    void applyEdit (const EditCallbacks::Change& change);
    void beginGesture();
    void updateGesture (const EditCallbacks::Change& change);
    void endGesture();

    // Changes to the project (new, deleted, muted tracks...), with undo.
    void applyProjectEdit (const std::function<void (model::Project&)>& change, std::optional<std::size_t> newActive = {});
    void setProject (const model::Project& newProject, std::size_t newActive);  // undo, redo, open
    void setActiveTrack (std::size_t index);
    void projectChanged();

    model::Track& activeTrack() noexcept             { return project.tracks[active]; }
    const model::Track& activeTrack() const noexcept { return project.tracks[active]; }

    void addTrack();
    void duplicateTrack (std::size_t index);
    void deleteTrack (std::size_t index);

    void rebuildSchedules();
    void showRandomMenu();

    // Export
    enum class ExportKind { Wav, Midi };
    void showExportMenu();
    void exportProject (ExportKind kind);
    model::Project projectForExport() const;

    // Sound samples
    void syncSamples();                       // decodes new ones and hands them to the engine
    void loadSample (bool forNode);           // "Load sample…"
    std::shared_ptr<const juce::AudioBuffer<float>> decodeSample (const juce::File& file, double& sampleRate,
                                                                  juce::String& error);
    void togglePlayPause();
    void updateUndoButtons();

    // Files
    void newProject();
    void openProject();
    void saveProject (std::function<void (bool saved)> done = {});
    void saveProjectAs (std::function<void (bool saved)> done = {});
    bool writeFile (const juce::File& file);
    void markSaved();
    bool hasUnsavedChanges() const;
    void updateTitle();
    juce::File defaultFolder() const;
    juce::String projectDisplayName() const;

    // Canvas area not covered by the panels (to frame the graph).
    juce::Rectangle<int> freeCanvasArea() const;

    // Audio
    audio::AudioEngine engine;
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer audioPlayer;

    // Data: the project with all its tracks; `active` is the one being edited.
    model::Project project;
    std::size_t active = 0;
    std::vector<std::shared_ptr<const engine::Schedule>> schedules;   // one per track
    juce::UndoManager undoManager;
    std::optional<model::Project> gestureStart;

    // Open file (empty = untitled) and the last saved state, to know if there are changes.
    juce::File currentFile;
    model::Project savedProject;
    double savedBpm = 120.0;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Rounded background of the floating bars.
    struct FloatingPanel : public juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            // Pill shape: the radius is half the height.
            const auto radius = static_cast<float> (getHeight()) * 0.5f;
            g.setColour (theme::panel);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), radius);
            g.setColour (theme::panelOutline);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), radius, 1.0f);
        }
    };

    // UI
    FloatingPanel topBar;      // file: new, open, save, export and the project name
    FloatingPanel bottomBar;
    FloatingPanel statusBar;   // warnings (unreachable nodes, audio): only when there is one
    TransportButton stopButton { "Stop", TransportButton::Icon::Stop };
    TransportButton playButton { "Play", TransportButton::Icon::Play };
    TransportButton pauseButton { "Pause", TransportButton::Icon::Pause };
    juce::Label bpmLabel { {}, "BPM" };
    juce::Slider bpmSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    IconButton randomButton { "Random", IconButton::Icon::Random };
    IconButton newButton { "New (Ctrl+N)", IconButton::Icon::New };
    IconButton openButton { "Open (Ctrl+O)", IconButton::Icon::Open };
    IconButton saveButton { "Save (Ctrl+S)", IconButton::Icon::Save };
    IconButton exportButton { "Export to WAV or MIDI", IconButton::Icon::Export };
    IconButton undoButton { "Undo (Ctrl+Z)", IconButton::Icon::Undo };
    IconButton redoButton { "Redo (Ctrl+Y)", IconButton::Icon::Redo };
    juce::Label statusLabel;
    juce::Label projectLabel;
    GraphView graphView;
    Inspector inspector;
    TrackList trackList;
    juce::TooltipWindow tooltips { this, 600 };

    juce::String audioWarning;
    juce::String sampleWarning;   // samples that could not be found

    // Samples already decoded, by path: loading the same file twice does not read
    // it again, and undo / redo never touch the disk.
    struct DecodedSample
    {
        std::shared_ptr<const juce::AudioBuffer<float>> buffer;
        double sampleRate = 44100.0;
    };
    std::map<std::string, DecodedSample> decodedSamples;
    std::vector<model::Sample> syncedSamples;
    std::vector<audio::SampleData> sampleData;   // the ones that loaded, for export
    juce::AudioFormatManager formatManager;
    int timerTicks = 0;
    bool initialFitDone = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace chimedes::ui
