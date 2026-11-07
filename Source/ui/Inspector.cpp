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
    // Subdivisiones del ajuste al pulso, en el orden del desplegable.
    constexpr std::array<double, 4> quantizeSteps { 0.0, 1.0, 0.5, 0.25 };

    // Id del primer instrumento en los desplegables. En el del nodo, el id 1 es
    // "Como la pista" (instrumento 0 del modelo) y los instrumentos empiezan en 2.
    constexpr int nodeUsesTrackItem = 1;
    constexpr int loadSampleItem = 900;   // "Cargar muestra…" en los dos desplegables

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
    trackInstrumentBox.onChange = [this]
    {
        if (trackInstrumentBox.getSelectedId() == loadSampleItem)
        {
            trackInstrumentBox.setSelectedId (trackInstrumentItem, juce::dontSendNotification);
            if (onLoadSample)
                onLoadSample (false);
            return;
        }

        const auto id = static_cast<model::InstrumentId> (trackInstrumentBox.getSelectedId());
        callbacks.edit ([id] (model::Track& t) { t.instrument = id; });
    };
    directedToggle.onClick = [this]
    {
        const auto directed = directedToggle.getToggleState();
        callbacks.edit ([directed] (model::Track& t) { t.directed = directed; });
    };

    // --- Nodo --------------------------------------------------------------
    styleHeader (nodeHeader, "Nodo");
    styleCaption (notesLabel, "Notas");
    styleCaption (notesHint, "Clic o letras A–K: probar · Mayús: acorde");
    styleCaption (instrumentLabel, "Instrumento");
    styleCaption (durationLabel, "Duración (pulsos)");
    styleCaption (velocityLabel, "Intensidad");
    styleCaption (noSelectionLabel, "Selecciona un nodo para editarlo.");
    notesHint.setFont (juce::FontOptions (12.0f));
    notesValue.setJustificationType (juce::Justification::centredRight);
    notesValue.setColour (juce::Label::textColourId, theme::textPrimary);

    // Tocar el piano prueba la nota; Enter la guarda en el nodo (se puede deshacer).
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

    startButton.setButtonText (text ("Inicio"));
    endButton.setButtonText (text ("Fin"));
    deleteButton.setButtonText (text ("Eliminar nodo"));

    startButton.setTooltip (text ("Marcar este nodo como inicio"));
    endButton.setTooltip (text ("Marcar este nodo como fin"));

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
             &trackHeader, &modeBox, &treeBox, &traversalBox, &siblingsBox, &quantizeBox, &trackInstrumentBox,
             &directedToggle, &nodeHeader, &notesLabel, &notesValue, &piano, &notesHint, &instrumentLabel,
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

        // Arrastrando: cambio en vivo dentro del gesto. Si no (texto o teclado): una edición.
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
    trackInstrumentItem = static_cast<int> (model::instruments::isValid (track.instrument) ? track.instrument
                                                                                          : model::instruments::organ);
    trackInstrumentBox.setSelectedId (trackInstrumentItem, juce::dontSendNotification);
    directedToggle.setToggleState (track.directed, juce::dontSendNotification);

    // Las opciones del algoritmo no aplican en el modo línea de tiempo.
    const bool algorithm = track.mode == model::PlaybackMode::Algorithm;
    for (auto* c : std::initializer_list<juce::Component*> { &treeBox, &traversalBox, &siblingsBox, &quantizeBox })
        c->setEnabled (algorithm);

    const auto* n = track.findNode (selectedNode);
    const auto newNode = n != nullptr ? n->id : model::noNode;

    // Otro nodo: la propuesta del anterior se descarta y se apagan las notas de prueba.
    if (newNode != node)
    {
        piano.releaseAllKeys();
        piano.discardPending();
    }
    node = newNode;

    for (auto* c : std::initializer_list<juce::Component*> {
             &notesLabel, &notesValue, &piano, &notesHint, &instrumentLabel, &nodeInstrumentBox, &durationLabel,
             &durationSlider, &velocityLabel, &velocitySlider, &startButton, &endButton, &deleteButton })
        c->setVisible (n != nullptr);

    noSelectionLabel.setVisible (n == nullptr);

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

int Inspector::getIdealHeight() const
{
    return const_cast<Inspector*> (this)->layout (false);
}

void Inspector::resized()
{
    layout (true);
}

int Inspector::layout (bool apply)
{
    // Una sola rutina coloca los controles y mide el alto, para que nunca se desfasen.
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
    place (trackInstrumentBox, rowH);
    place (directedToggle, 22, 14);

    place (nodeHeader, 22);
    if (apply)
        noSelectionLabel.setBounds (padX, y, width, 22);

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
    // Con una propuesta sin guardar, las notas y la ayuda pasan a blanco estelar.
    const bool proposing = piano.hasPendingChange();

    notesValue.setText (juce::String (model::pitchNames (piano.getPendingPitches())), juce::dontSendNotification);
    notesValue.setColour (juce::Label::textColourId, proposing ? theme::accent : theme::textPrimary);

    notesHint.setText (text (proposing ? "Enter: guardar · Esc: descartar"
                                       : "Clic o letras A–K: probar · Mayús: acorde"),
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
}

void Inspector::rebuildInstrumentBoxes()
{
    // Integrados, luego las muestras del proyecto y al final "Cargar muestra…".
    trackInstrumentBox.clear (juce::dontSendNotification);
    nodeInstrumentBox.clear (juce::dontSendNotification);
    nodeInstrumentBox.addItem ("Como la pista", nodeUsesTrackItem);

    for (int id = model::instruments::first; id <= model::instruments::count; ++id)
    {
        const auto name = text (model::instruments::name (static_cast<model::InstrumentId> (id)));
        trackInstrumentBox.addItem (text ("Instrumento: ") + name, id);
        nodeInstrumentBox.addItem (name, id + 1);
    }

    if (! sampleChoices.empty())
    {
        trackInstrumentBox.addSeparator();
        nodeInstrumentBox.addSeparator();

        for (const auto& [id, name] : sampleChoices)
        {
            trackInstrumentBox.addItem (text ("Instrumento: ") + name, static_cast<int> (id));
            nodeInstrumentBox.addItem (name, static_cast<int> (id) + 1);
        }
    }

    const auto loadText = canLoadSample ? text ("Cargar muestra…") : text ("Cargar muestra… (máximo 10)");
    trackInstrumentBox.addSeparator();
    nodeInstrumentBox.addSeparator();
    trackInstrumentBox.addItem (loadText, loadSampleItem);
    nodeInstrumentBox.addItem (loadText, loadSampleItem);
    trackInstrumentBox.setItemEnabled (loadSampleItem, canLoadSample);
    nodeInstrumentBox.setItemEnabled (loadSampleItem, canLoadSample);

    trackInstrumentBox.setSelectedId (trackInstrumentItem, juce::dontSendNotification);
    nodeInstrumentBox.setSelectedId (nodeInstrumentItem, juce::dontSendNotification);
}

} // namespace chimedes::ui
