#include "Inspector.h"

#include <array>
#include <cmath>

#include "model/NoteNames.h"
#include "model/TrackEditing.h"
#include "ui/Text.h"
#include "ui/Theme.h"

namespace chimedes::ui
{

namespace
{
    // Subdivisiones del ajuste al pulso, en el orden del desplegable.
    constexpr std::array<double, 4> quantizeSteps { 0.0, 1.0, 0.5, 0.25 };

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
    // --- Pista -------------------------------------------------------------
    styleHeader (trackHeader, "Pista");

    modeBox.addItemList ({ text ("Línea de tiempo"), "Algoritmo" }, 1);
    treeBox.addItemList ({ text ("Árbol: BFS"), text ("Árbol: DFS"), text ("Árbol: Dijkstra"), text ("Árbol: Prim") }, 1);
    traversalBox.addItemList ({ "Recorrido: BFS", "Recorrido: DFS" }, 1);
    siblingsBox.addItemList ({ "Hermanos: en secuencia", "Hermanos: a la vez" }, 1);
    quantizeBox.addItemList ({ "Ajuste al pulso: libre", "Ajuste al pulso: 1", "Ajuste al pulso: 1/2", "Ajuste al pulso: 1/4" }, 1);
    directedToggle.setButtonText ("Grafo dirigido");

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
    directedToggle.onClick = [this]
    {
        const auto directed = directedToggle.getToggleState();
        callbacks.edit ([directed] (model::Track& t) { t.directed = directed; });
    };

    // --- Nodo --------------------------------------------------------------
    styleHeader (nodeHeader, "Nodo");
    styleCaption (notesLabel, "Notas");
    styleCaption (notesHint, "Ej.: C4 · F#3 · C4 E4 G4 (acorde)");
    styleCaption (durationLabel, "Duración (pulsos)");
    styleCaption (velocityLabel, "Intensidad");
    styleCaption (noSelectionLabel, "Selecciona un nodo para editarlo.");
    notesHint.setFont (juce::FontOptions (12.0f));

    notesEditor.onReturnKey = [this] { applyNotes(); };
    notesEditor.onFocusLost = [this] { applyNotes(); };
    notesEditor.onEscapeKey = [this] { notesEditor.setText (notesShown, false); resetNotesColour(); };

    durationSlider.setRange (0.25, 8.0, 0.25);
    velocitySlider.setRange (1.0, 127.0, 1.0);
    durationSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
    velocitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);

    connectSlider (durationSlider, [] (model::Node& n, double v) { n.durationBeats = v; });
    connectSlider (velocitySlider, [] (model::Node& n, double v) { n.velocity = static_cast<int> (v); });

    startButton.setButtonText (text ("Marcar como inicio"));
    endButton.setButtonText (text ("Marcar como fin"));
    deleteButton.setButtonText (text ("Eliminar nodo"));

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
             &trackHeader, &modeBox, &treeBox, &traversalBox, &siblingsBox, &quantizeBox, &directedToggle,
             &nodeHeader, &notesLabel, &notesEditor, &notesHint, &durationLabel, &durationSlider,
             &velocityLabel, &velocitySlider, &startButton, &endButton, &deleteButton, &noSelectionLabel })
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

        // Arrastrando: cambio en vivo dentro del gesto. Si no (texto o teclado): una edición.
        if (slider.isMouseButtonDown())
            callbacks.updateGesture (change);
        else
            callbacks.edit (change);
    };
}

void Inspector::applyNotes()
{
    if (node == model::noNode || notesEditor.getText() == notesShown)
        return;

    const auto pitches = model::parsePitches (notesEditor.getText().toStdString());

    if (! pitches.has_value())
    {
        notesEditor.setColour (juce::TextEditor::outlineColourId, theme::error);
        notesEditor.setColour (juce::TextEditor::focusedOutlineColourId, theme::error);
        notesEditor.repaint();
        return;
    }

    resetNotesColour();
    editNode ([p = *pitches] (model::Node& n) { n.pitches = p; });
}

void Inspector::resetNotesColour()
{
    // Vuelve a los colores del tema quitando los que se pusieron al marcar el error.
    notesEditor.removeColour (juce::TextEditor::outlineColourId);
    notesEditor.removeColour (juce::TextEditor::focusedOutlineColourId);
    notesEditor.repaint();
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
    directedToggle.setToggleState (track.directed, juce::dontSendNotification);

    // Las opciones del algoritmo no aplican en el modo línea de tiempo.
    const bool algorithm = track.mode == model::PlaybackMode::Algorithm;
    for (auto* c : std::initializer_list<juce::Component*> { &treeBox, &traversalBox, &siblingsBox, &quantizeBox })
        c->setEnabled (algorithm);

    const auto* n = track.findNode (selectedNode);
    node = n != nullptr ? n->id : model::noNode;

    for (auto* c : std::initializer_list<juce::Component*> {
             &notesLabel, &notesEditor, &notesHint, &durationLabel, &durationSlider,
             &velocityLabel, &velocitySlider, &startButton, &endButton, &deleteButton })
        c->setVisible (n != nullptr);

    noSelectionLabel.setVisible (n == nullptr);

    if (n == nullptr)
        return;

    notesShown = juce::String (model::pitchNames (n->pitches));

    // No se pisa lo que el usuario está escribiendo.
    if (! notesEditor.hasKeyboardFocus (true))
    {
        notesEditor.setText (notesShown, false);
        resetNotesColour();
    }

    durationSlider.setValue (n->durationBeats, juce::dontSendNotification);
    velocitySlider.setValue (n->velocity, juce::dontSendNotification);
    startButton.setEnabled (track.startNode != n->id);
    endButton.setEnabled (track.endNode != n->id);
}

void Inspector::paint (juce::Graphics& g)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), theme::cornerRadius);
}

int Inspector::getIdealHeight() const
{
    // Debe coincidir con resized(): márgenes + filas + separaciones.
    return 12 + (24 + 6) + 5 * (30 + 6) + (24 + 18)
         + (24 + 6) + (18 + 2) + (30 + 2) + (16 + 10) + (18 + 2) + (24 + 10)
         + (18 + 2) + (24 + 16) + 3 * (30 + 6) + 6;
}

void Inspector::resized()
{
    auto area = getLocalBounds().reduced (14, 12);

    const auto row = [&area] (juce::Component& c, int height, int gapAfter = 6)
    {
        c.setBounds (area.removeFromTop (height));
        area.removeFromTop (gapAfter);
    };

    row (trackHeader, 24);
    row (modeBox, 30);
    row (treeBox, 30);
    row (traversalBox, 30);
    row (siblingsBox, 30);
    row (quantizeBox, 30);
    row (directedToggle, 24, 18);

    row (nodeHeader, 24);
    noSelectionLabel.setBounds (area.withHeight (24));

    row (notesLabel, 18, 2);
    row (notesEditor, 30, 2);
    row (notesHint, 16, 10);
    row (durationLabel, 18, 2);
    row (durationSlider, 24, 10);
    row (velocityLabel, 18, 2);
    row (velocitySlider, 24, 16);
    row (startButton, 30);
    row (endButton, 30);
    row (deleteButton, 30);
}

} // namespace chimedes::ui
