#include "Inspector.h"

#include <array>
#include <cmath>

#include "model/Instruments.h"
#include "model/NoteNames.h"
#include "model/TrackEditing.h"
#include "ui/Text.h"
#include "ui/Theme.h"

namespace chimedes::ui
{

namespace
{
    // Beat snapping subdivisions, in drop-down order.
    constexpr std::array<double, 4> quantizeSteps { 0.0, 1.0, 0.5, 0.25 };

    // Id of the first instrument in the drop-downs. In the node's, id 1 is
    // "Same as track" (model instrument 0) and instruments start at 2.
    constexpr int nodeUsesTrackItem = 1;
    constexpr int loadSampleItem = 900;   // "Load sample…" in both drop-downs

    int quantizeIndex (double beats)
    {
        for (std::size_t i = 0; i < quantizeSteps.size(); ++i)
            if (std::abs (quantizeSteps[i] - beats) < 1e-9)
                return static_cast<int> (i);
        return 0;
    }

    void styleHeader (juce::Label& label, const char* name)
    {
        label.setText (text (name), juce::dontSendNotification);
        label.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, theme::textPrimary);
    }

    void styleCaption (juce::Label& label, const char* name)
    {
        label.setText (text (name), juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, theme::textSecondary);
    }
}

Inspector::Inspector()
{
    // --- Track -------------------------------------------------------------
    styleHeader (trackHeader, "Track");

    modeBox.addItemList ({ "Timeline", "Algorithm" }, 1);
    treeBox.addItemList ({ "Tree: BFS", "Tree: DFS", "Tree: Dijkstra", "Tree: Prim" }, 1);
    traversalBox.addItemList ({ "Traversal: BFS", "Traversal: DFS" }, 1);
    siblingsBox.addItemList ({ "Siblings: in sequence", "Siblings: together" }, 1);
    quantizeBox.addItemList ({ "Snap to beat: off", "Snap to beat: 1", "Snap to beat: 1/2", "Snap to beat: 1/4" }, 1);
    directedToggle.setButtonText ("Directed graph");

    rebuildInstrumentBoxes();

    modeBox.onChange = [this]
    {
        const auto mode = modeBox.getSelectedItemIndex() == 0 ? model::PlaybackMode::Timeline : model::PlaybackMode::Algorithm;
        callbacks.edit ([mode] (model::Track& t) { t.mode = mode; });
    };
    treeBox.onChange = [this]
    {
        const auto method = static_cast<model::TreeMethod> (treeBox.getSelectedItemIndex());
        callbacks.edit ([method] (model::Track& t) { t.treeMethod = method; });
    };
    traversalBox.onChange = [this]
    {
        const auto traversal = static_cast<model::Traversal> (traversalBox.getSelectedItemIndex());
        callbacks.edit ([traversal] (model::Track& t) { t.traversal = traversal; });
    };
    siblingsBox.onChange = [this]
    {
        const auto siblings = static_cast<model::SiblingMode> (siblingsBox.getSelectedItemIndex());
        callbacks.edit ([siblings] (model::Track& t) { t.siblings = siblings; });
    };
    quantizeBox.onChange = [this]
    {
        const auto step = quantizeSteps[static_cast<std::size_t> (std::max (0, quantizeBox.getSelectedItemIndex()))];
        callbacks.edit ([step] (model::Track& t) { t.quantizeBeats = step; });
    };
    // The track's instrument: an always-open list with icons, in the space left over.
    styleCaption (trackInstrumentLabel, "Instrument");
    instrumentList.onSelect = [this] (model::InstrumentId id)
    {
        callbacks.edit ([id] (model::Track& t) { t.instrument = id; });
    };
    instrumentList.onLoadSample = [this]
    {
        if (onLoadSample)
            onLoadSample (false);
    };
    instrumentViewport.setViewedComponent (&instrumentList, false);
    instrumentViewport.setScrollBarsShown (true, false);
    instrumentViewport.setScrollBarThickness (6);
    instrumentViewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, theme::controlPressed);
    directedToggle.onClick = [this]
    {
        const auto directed = directedToggle.getToggleState();
        callbacks.edit ([directed] (model::Track& t) { t.directed = directed; });
    };

    // --- Node --------------------------------------------------------------
    styleHeader (nodeHeader, "Node");
    styleCaption (notesLabel, "Notes");
    styleCaption (notesHint, "Click or keys A–K: try · Shift: chord");
    styleCaption (instrumentLabel, "Instrument");
    styleCaption (durationLabel, "Duration (beats)");
    styleCaption (velocityLabel, "Velocity");
    styleCaption (noSelectionLabel, "Select a node to edit it.");
    notesHint.setFont (juce::FontOptions (12.0f));
    notesValue.setJustificationType (juce::Justification::centredRight);
    notesValue.setColour (juce::Label::textColourId, theme::textPrimary);

    // Playing the piano previews the note; Enter saves it to the node (undoable).
    piano.onCommit = [this] (std::vector<int> pitches)
    {
        editNode ([p = std::move (pitches)] (model::Node& n) { n.pitches = p; });
    };
    piano.onAudition = [this] (int pitch, bool isDown)
    {
        if (onAudition)
            onAudition (pitch, isDown ? auditionVelocity : 0, auditionInstrument);
    };
    piano.onPendingChanged = [this] { updateNotesDisplay(); };

    nodeInstrumentBox.onChange = [this]
    {
        const auto item = nodeInstrumentBox.getSelectedId();
        if (item == loadSampleItem)
        {
            nodeInstrumentBox.setSelectedId (nodeInstrumentItem, juce::dontSendNotification);
            if (onLoadSample)
                onLoadSample (true);
            return;
        }

        const auto id = static_cast<model::InstrumentId> (item == nodeUsesTrackItem ? 0 : item - 1);
        editNode ([id] (model::Node& n) { n.instrument = id; });
    };

    durationSlider.setRange (0.25, 8.0, 0.25);
    velocitySlider.setRange (1.0, 127.0, 1.0);
    durationSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
    velocitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);

    connectSlider (durationSlider, [] (model::Node& n, double v) { n.durationBeats = v; });
    connectSlider (velocitySlider, [] (model::Node& n, double v) { n.velocity = static_cast<int> (v); });

    startButton.setButtonText ("Start");
    endButton.setButtonText ("End");
    deleteButton.setButtonText ("Delete node");

    startButton.setTooltip ("Mark this node as the start");
    endButton.setTooltip ("Mark this node as the end");

    startButton.onClick = [this]
    {
        const auto id = node;
        callbacks.edit ([id] (model::Track& t) { t.startNode = id; });
    };
    endButton.onClick = [this]
    {
        const auto id = node;
        callbacks.edit ([id] (model::Track& t) { t.endNode = id; });
    };
    deleteButton.onClick = [this]
    {
        const auto id = node;
        callbacks.edit ([id] (model::Track& t) { model::removeNode (t, id); });
    };

    for (auto* c : std::initializer_list<juce::Component*> {
             &trackHeader, &modeBox, &treeBox, &traversalBox, &siblingsBox, &quantizeBox,
             &directedToggle, &trackInstrumentLabel, &instrumentViewport, &nodeHeader, &notesLabel, &notesValue, &piano, &notesHint, &instrumentLabel,
             &nodeInstrumentBox, &durationLabel, &durationSlider, &velocityLabel, &velocitySlider,
             &startButton, &endButton, &deleteButton, &noSelectionLabel })
        addAndMakeVisible (c);
}

void Inspector::connectSlider (juce::Slider& slider, std::function<void (model::Node&, double)> apply)
{
    slider.onDragStart = [this] { callbacks.beginGesture(); };
    slider.onDragEnd = [this] { callbacks.endGesture(); };

    slider.onValueChange = [this, &slider, apply]
    {
        const auto id = node;
        const auto value = slider.getValue();
        const auto change = [id, value, apply] (model::Track& t)
        {
            if (auto* n = model::findNodeMutable (t, id))
                apply (*n, value);
        };

        // While dragging: a live change inside the gesture. Otherwise (text or keyboard): one edit.
        if (slider.isMouseButtonDown())
            callbacks.updateGesture (change);
        else
            callbacks.edit (change);
    };
}

void Inspector::editNode (const std::function<void (model::Node&)>& change)
{
    const auto id = node;
    callbacks.edit ([id, change] (model::Track& t)
    {
        if (auto* n = model::findNodeMutable (t, id))
            change (*n);
    });
}

void Inspector::refresh (const model::Track& track, model::NodeId selectedNode)
{
    modeBox.setSelectedItemIndex (track.mode == model::PlaybackMode::Timeline ? 0 : 1, juce::dontSendNotification);
    treeBox.setSelectedItemIndex (static_cast<int> (track.treeMethod), juce::dontSendNotification);
    traversalBox.setSelectedItemIndex (static_cast<int> (track.traversal), juce::dontSendNotification);
    siblingsBox.setSelectedItemIndex (static_cast<int> (track.siblings), juce::dontSendNotification);
    quantizeBox.setSelectedItemIndex (quantizeIndex (track.quantizeBeats), juce::dontSendNotification);
    {
        const auto instrument = model::instruments::isValid (track.instrument) ? track.instrument : model::instruments::organ;
        if (instrument != instrumentList.getSelected())
        {
            instrumentList.setSelected (instrument);

            // Changed from elsewhere (undo, the dice, another track): bring it into view.
            const auto row = instrumentList.rowRange (instrument);
            const auto top = instrumentViewport.getViewPositionY();
            const auto visible = instrumentViewport.getMaximumVisibleHeight();
            if (! row.isEmpty() && (row.getStart() < top || row.getEnd() > top + visible))
                instrumentViewport.setViewPosition (0, std::max (0, row.getStart() - (visible - row.getLength()) / 2));
        }
    }
    directedToggle.setToggleState (track.directed, juce::dontSendNotification);

    // Algorithm options do not apply in timeline mode.
    const bool algorithm = track.mode == model::PlaybackMode::Algorithm;
    for (auto* c : std::initializer_list<juce::Component*> { &treeBox, &traversalBox, &siblingsBox, &quantizeBox })
        c->setEnabled (algorithm);

    const auto* n = track.findNode (selectedNode);
    const auto newNode = n != nullptr ? n->id : model::noNode;

    // Another node: the previous proposal is discarded and preview notes are turned off.
    if (newNode != node)
    {
        piano.releaseAllKeys();
        piano.discardPending();
    }
    const bool selectionShown = node != model::noNode;
    node = newNode;

    for (auto* c : std::initializer_list<juce::Component*> {
             &notesLabel, &notesValue, &piano, &notesHint, &instrumentLabel, &nodeInstrumentBox, &durationLabel,
             &durationSlider, &velocityLabel, &velocitySlider, &startButton, &endButton, &deleteButton })
        c->setVisible (n != nullptr);

    noSelectionLabel.setVisible (n == nullptr);

    // The node section grows or shrinks: the instrument list gets what is left.
    if (selectionShown != (n != nullptr))
        resized();

    if (n == nullptr)
        return;

    auditionVelocity = n->velocity;
    auditionInstrument = static_cast<int> (model::resolveInstrument (track, *n));
    piano.setPitches (n->pitches);
    updateNotesDisplay();
    nodeInstrumentItem = model::instruments::isValid (n->instrument) ? static_cast<int> (n->instrument) + 1
                                                                     : nodeUsesTrackItem;
    nodeInstrumentBox.setSelectedId (nodeInstrumentItem, juce::dontSendNotification);
    durationSlider.setValue (n->durationBeats, juce::dontSendNotification);
    velocitySlider.setValue (n->velocity, juce::dontSendNotification);
    startButton.setEnabled (track.startNode != n->id);
    endButton.setEnabled (track.endNode != n->id);
}

void Inspector::paint (juce::Graphics& g)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), theme::cornerRadius);
    g.setColour (theme::panelOutline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), theme::cornerRadius, 1.0f);
}

int Inspector::fixedHeight() const
{
    return const_cast<Inspector*> (this)->layout (false, 0);
}

int Inspector::getIdealHeight() const
{
    return fixedHeight() + instrumentList.getContentHeight();
}

void Inspector::resized()
{
    constexpr int minimumList = 3 * InstrumentList::rowHeight;
    layout (true, std::max (minimumList, getHeight() - fixedHeight()));
}

int Inspector::layout (bool apply, int listHeight)
{
    // A single routine places the controls and measures the height, so they never drift apart.
    constexpr int padX = 14, padY = 12, rowH = 28;
    const auto width = getWidth() - 2 * padX;
    int y = padY;

    const auto place = [&] (juce::Component& c, int height, int gapAfter = 4)
    {
        if (apply)
            c.setBounds (padX, y, width, height);
        y += height + gapAfter;
    };

    place (trackHeader, 22);
    place (modeBox, rowH);
    place (treeBox, rowH);
    place (traversalBox, rowH);
    place (siblingsBox, rowH);
    place (quantizeBox, rowH);
    place (directedToggle, 22, 8);

    place (trackInstrumentLabel, 16, 2);
    if (apply)
    {
        instrumentViewport.setBounds (padX - 4, y, width + 8, listHeight);
        instrumentList.setSize (instrumentViewport.getMaximumVisibleWidth()
                                    - (instrumentList.getContentHeight() > listHeight ? 8 : 0),
                                instrumentList.getContentHeight());
    }
    y += listHeight + 14;

    place (nodeHeader, 22);
    if (apply)
        noSelectionLabel.setBounds (padX, y, width, 22);

    // With no node selected, the section is only that line.
    if (node == model::noNode)
        return y + 22 + padY;

    if (apply)
    {
        notesLabel.setBounds (padX, y, 60, 18);
        notesValue.setBounds (padX + 60, y, width - 60, 18);
    }
    y += 18 + 2;

    place (piano, 84, 2);
    place (notesHint, 16, 8);
    place (instrumentLabel, 16, 2);
    place (nodeInstrumentBox, rowH, 8);
    place (durationLabel, 16, 0);
    place (durationSlider, 24, 6);
    place (velocityLabel, 16, 0);
    place (velocitySlider, 24, 10);

    if (apply)
    {
        const auto half = (width - 6) / 2;
        startButton.setBounds (padX, y, half, rowH);
        endButton.setBounds (padX + half + 6, y, width - half - 6, rowH);
    }
    y += rowH + 4;

    place (deleteButton, rowH, 0);
    return y + padY;
}

void Inspector::updateNotesDisplay()
{
    // With an unsaved proposal, the notes and the hint turn starlight white.
    const bool proposing = piano.hasPendingChange();

    notesValue.setText (juce::String (model::pitchNames (piano.getPendingPitches())), juce::dontSendNotification);
    notesValue.setColour (juce::Label::textColourId, proposing ? theme::accent : theme::textPrimary);

    notesHint.setText (text (proposing ? "Enter: save · Esc: discard"
                                       : "Click or keys A–K: try · Shift: chord"),
                       juce::dontSendNotification);
    notesHint.setColour (juce::Label::textColourId, proposing ? theme::accent : theme::textSecondary);
}

bool Inspector::handlePianoKeyPress (const juce::KeyPress& key)
{
    return node != model::noNode && piano.handleKeyPress (key);
}

bool Inspector::handlePianoKeyStateChange()
{
    if (node == model::noNode)
        return false;

    return piano.handleKeyStateChange();
}

void Inspector::setSampleChoices (std::vector<std::pair<model::InstrumentId, juce::String>> samples, bool canLoadMore)
{
    if (samples == sampleChoices && canLoadMore == canLoadSample)
        return;

    sampleChoices = std::move (samples);
    canLoadSample = canLoadMore;
    rebuildInstrumentBoxes();
    resized();   // the list's height changed
}

void Inspector::rebuildInstrumentBoxes()
{
    // The track's list, then the node's drop-down: built-ins, the project's
    // samples and finally "Load sample…".
    instrumentList.setSamples (sampleChoices, canLoadSample);
    nodeInstrumentBox.clear (juce::dontSendNotification);
    nodeInstrumentBox.addItem ("Same as track", nodeUsesTrackItem);

    // Grouped by category, each group under its heading.
    for (std::size_t c = 0; c < model::instruments::categoryNames.size(); ++c)
    {
        const auto category = static_cast<model::instruments::Category> (c);
        const auto heading = text (model::instruments::categoryNames[c]);
        nodeInstrumentBox.addSectionHeading (heading);

        for (const auto& info : model::instruments::builtIns)
        {
            if (info.category != category)
                continue;

            const auto id = static_cast<int> (info.id);
            const auto name = text (info.name);
            nodeInstrumentBox.addItem (name, id + 1);
        }
    }

    if (! sampleChoices.empty())
    {
        nodeInstrumentBox.addSeparator();

        for (const auto& [id, name] : sampleChoices)
        {
            nodeInstrumentBox.addItem (name, static_cast<int> (id) + 1);
        }
    }

    const auto loadText = canLoadSample ? text ("Load sample…") : text ("Load sample… (10 max)");
    nodeInstrumentBox.addSeparator();
    nodeInstrumentBox.addItem (loadText, loadSampleItem);
    nodeInstrumentBox.setItemEnabled (loadSampleItem, canLoadSample);

    nodeInstrumentBox.setSelectedId (nodeInstrumentItem, juce::dontSendNotification);
}

} // namespace chimedes::ui
