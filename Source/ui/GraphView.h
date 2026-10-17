#pragma once

#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Schedule.h"
#include "model/Track.h"

namespace chimedes::ui
{

// Dibuja el grafo de una pista sobre la regla de tiempo, la línea de reproducción y
// los nodos que están sonando. Por ahora es de solo lectura: la edición con el ratón
// llega en el siguiente paso de la fase 1.
class GraphView : public juce::Component
{
public:
    static constexpr int rulerHeight = 28;
    static constexpr float originX = 40.0f;   // dónde cae el píxel 0 del lienzo
    static constexpr float originY = 60.0f;
    static constexpr float nodeRadius = 18.0f;

    GraphView();

    void setTrack (const model::Track* newTrack);
    void setSchedule (std::shared_ptr<const engine::Schedule> newSchedule);
    void setUnreachable (std::vector<model::NodeId> nodes);

    // Posición dentro del ciclo, en pulsos. `isPlaying` decide si se iluminan nodos.
    void setPlayhead (double beatInCycle, bool isPlaying);

    void paint (juce::Graphics& g) override;

private:
    juce::Point<float> toScreen (const model::Node& node) const noexcept;

    void paintRuler (juce::Graphics& g) const;
    void paintEdges (juce::Graphics& g) const;
    void paintNodes (juce::Graphics& g) const;
    void paintPlayhead (juce::Graphics& g) const;

    bool isSounding (model::NodeId node) const noexcept;
    bool isUnreachable (model::NodeId node) const noexcept;

    const model::Track* track = nullptr;
    std::shared_ptr<const engine::Schedule> schedule;
    std::vector<model::NodeId> unreachable;
    double playheadBeat = 0.0;
    bool playing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphView)
};

} // namespace chimedes::ui
