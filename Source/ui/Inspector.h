#pragma once

#include <functional>
#include <utility>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "model/Track.h"
#include "ui/EditCallbacks.h"
#include "ui/InstrumentList.h"
#include "ui/PianoPicker.h"

namespace chimedes::ui
{

// Right panel: properties of the track and of the selected node.
// It keeps no state of its own: refresh() reads it again from the track, and
// every change goes through EditCallbacks so it can be undone.
class Inspector : public juce::Component
{
public:
    Inspector();

    EditCallbacks callbacks;

    // Shows the track and the selected node again (noNode = none).
    void refresh (const model::Track& track, model::NodeId selectedNode);

    // Height it needs to show all of its content (the instrument list fully open).
    int getIdealHeight() const;

    // Keyboard preview notes: velocity 0 = off. The instrument is already
    // resolved (the node's or the track's).
    std::function<void (int pitch, int velocity, int instrument)> onAudition;

    // The project's samples for the instrument drop-downs (id and name).
    // `canLoadMore` enables "Load sample…" (there are ten at most).
    void setSampleChoices (std::vector<std::pair<model::InstrumentId, juce::String>> samples, bool canLoadMore);

    // "Load sample…" in the track's instrument list (false) or the node's drop-down (true).
    std::function<void (bool forNode)> onLoadSample;

    // Computer keyboard for the piano (only with a node selected).
    bool handlePianoKeyPress (const juce::KeyPress& key);
    bool handlePianoKeyStateChange();
    void releasePianoKeys() { piano.releaseAllKeys(); }

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    // Places the controls (if apply) and returns the height without the
    // instrument list, which takes whatever is left (`listHeight`).
    int layout (bool apply, int listHeight);
    int fixedHeight() const;
    void editNode (const std::function<void (model::Node&)>& change);
    void updateNotesDisplay();
    void rebuildInstrumentBoxes();
    void connectSlider (juce::Slider& slider, std::function<void (model::Node&, double)> apply);

    model::NodeId node = model::noNode;
    int auditionVelocity = 100;
    int auditionInstrument = 1;

    std::vector<std::pair<model::InstrumentId, juce::String>> sampleChoices;
    bool canLoadSample = true;
    int nodeInstrumentItem = 1;   // the chosen one, to go back to it after "Load sample…"

    // Track
    juce::Label trackHeader;
    juce::ComboBox modeBox, treeBox, traversalBox, siblingsBox, quantizeBox;
    juce::ToggleButton directedToggle;
    juce::Label trackInstrumentLabel;
    InstrumentList instrumentList;
    juce::Viewport instrumentViewport;

    // Node
    juce::Label nodeHeader, notesLabel, notesValue, notesHint, instrumentLabel, durationLabel, velocityLabel;
    PianoPicker piano;
    juce::ComboBox nodeInstrumentBox;
    juce::Slider durationSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider velocitySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton startButton, endButton, deleteButton;
    juce::Label noSelectionLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Inspector)
};

} // namespace chimedes::ui
