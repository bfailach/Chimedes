#pragma once

#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Schedule.h"
#include "model/Track.h"
#include "ui/EditCallbacks.h"

namespace chimedes::ui
{

// Lienzo del grafo: la regla de tiempo, los nodos y aristas, la línea de reproducción
// y los nodos que suenan. También es el editor:
//
//   doble clic en vacío ........ crear nodo
//   arrastrar el centro ........ mover nodo
//   arrastrar el borde ......... conectar con otro nodo
//   clic en nodo o arista ...... seleccionar (Supr borra)
//   clic derecho ............... marcar inicio o fin, eliminar
//   rueda ...................... zoom
//   arrastrar en vacío ......... desplazarse (también con el botón central)
class GraphView : public juce::Component
{
public:
    static constexpr int rulerHeight = 28;
    static constexpr float nodeRadius = 18.0f;

    GraphView();

    void setTrack (const model::Track* newTrack);
    void setSchedule (std::shared_ptr<const engine::Schedule> newSchedule);
    void setUnreachable (std::vector<model::NodeId> nodes);
    void setPlayhead (double beatInCycle, bool isPlaying);

    EditCallbacks callbacks;
    std::function<void (model::NodeId)> onSelectionChanged;

    model::NodeId getSelectedNode() const noexcept;
    void selectNode (model::NodeId node);
    void deleteSelection();

    // juce::Component
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    enum class Drag { None, MovingNode, Connecting, Panning };

    // Coordenadas: el modelo usa píxeles con zoom al 100 %; la pantalla aplica zoom
    // y desplazamiento. `viewOrigin` es el punto del modelo en la esquina superior
    // izquierda del área bajo la regla.
    juce::Point<float> toScreen (double x, double y) const noexcept;
    juce::Point<float> toScreen (const model::Node& node) const noexcept;
    juce::Point<double> toModel (juce::Point<float> screen) const noexcept;

    const model::Node* nodeAt (juce::Point<float> screen) const noexcept;
    bool isOnRing (const model::Node& node, juce::Point<float> screen) const noexcept;
    model::EdgeId edgeAt (juce::Point<float> screen) const noexcept;

    void showNodeMenu (model::NodeId node);
    void setSelection (model::NodeId node, model::EdgeId edge);

    void paintRuler (juce::Graphics& g) const;
    void paintEdges (juce::Graphics& g) const;
    void paintNodes (juce::Graphics& g) const;
    void paintPlayhead (juce::Graphics& g) const;
    void paintConnectionPreview (juce::Graphics& g) const;
    void paintEmptyHint (juce::Graphics& g) const;

    bool isSounding (model::NodeId node) const noexcept;
    bool isUnreachable (model::NodeId node) const noexcept;

    const model::Track* track = nullptr;
    std::shared_ptr<const engine::Schedule> schedule;
    std::vector<model::NodeId> unreachable;
    double playheadBeat = 0.0;
    bool playing = false;

    juce::Point<double> viewOrigin { -40.0, -40.0 };
    float zoom = 1.0f;

    model::NodeId selectedNode = model::noNode;
    model::EdgeId selectedEdge = 0;

    // Notas y duración que heredan los nodos nuevos (las del último seleccionado).
    std::vector<int> lastPitches { 60 };
    double lastDuration = 1.0;

    Drag drag = Drag::None;
    model::NodeId dragNode = model::noNode;
    juce::Point<double> dragOffset;         // del ratón al centro del nodo, en el modelo
    juce::Point<double> panStartOrigin;
    juce::Point<float> mousePosition;
    model::NodeId hoverTarget = model::noNode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphView)
};

} // namespace chimedes::ui
