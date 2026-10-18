#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "model/Track.h"
#include "ui/EditCallbacks.h"

namespace chimedes::ui
{

// Panel derecho: propiedades de la pista y del nodo seleccionado.
// No guarda estado propio: refresh() lo vuelve a leer de la pista, y cada cambio
// se pide por EditCallbacks para que se pueda deshacer.
class Inspector : public juce::Component
{
public:
    Inspector();

    EditCallbacks callbacks;

    // Vuelve a mostrar la pista y el nodo seleccionado (noNode = ninguno).
    void refresh (const model::Track& track, model::NodeId selectedNode);

    // Alto que necesita para mostrar todo su contenido.
    int getIdealHeight() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void applyNotes();
    void resetNotesColour();
    void editNode (const std::function<void (model::Node&)>& change);
    void connectSlider (juce::Slider& slider, std::function<void (model::Node&, double)> apply);

    model::NodeId node = model::noNode;
    juce::String notesShown;

    // Pista
    juce::Label trackHeader;
    juce::ComboBox modeBox, treeBox, traversalBox, siblingsBox, quantizeBox;
    juce::ToggleButton directedToggle;

    // Nodo
    juce::Label nodeHeader, notesLabel, notesHint, durationLabel, velocityLabel;
    juce::TextEditor notesEditor;
    juce::Slider durationSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider velocitySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton startButton, endButton, deleteButton;
    juce::Label noSelectionLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Inspector)
};

} // namespace chimedes::ui
