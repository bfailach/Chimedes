#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Schedule.h"
#include "model/Track.h"
#include "ui/EditCallbacks.h"

namespace chimedes::ui
{

// Graph canvas, drawn as a star atlas: each node is a star whose size follows its
// velocity. During playback, a spark travels along the edge towards the next note
// and the star flashes in its note's colour (see theme::pitchColour), with ripples
// in neighbouring colours; the path played stays as a pastel trail until the next
// cycle starts.
//
// With several tracks, all of them are drawn and animated at once; the active track
// is drawn in full and is the only one that can be edited, the others are dimmed (a
// click on one of their stars makes that track active). It is also the editor:
//
//   double-click on empty space .. create node
//   drag the centre .............. move node
//   drag the edge ................ connect to another node
//   click a node or edge ......... select (Delete removes it)
//   right-click .................. mark as start or end, delete
//   wheel ........................ zoom
//   drag on empty space .......... pan (also with the middle button)
class GraphView : public juce::Component
{
public:
    static constexpr int rulerHeight = 28;
    static constexpr float nodeRadius = 20.0f;

    GraphView();

    // A track to draw. The pointers must stay alive until the next call to
    // setTracks (the window repeats it after every change).
    struct TrackView
    {
        const model::Track* track = nullptr;
        std::shared_ptr<const engine::Schedule> schedule;   // nullptr = silent
        std::vector<model::NodeId> unreachable;
    };

    void setTracks (std::vector<TrackView> tracks, std::size_t activeIndex);

    // Global position in beats since playback started; each track converts it to
    // its own cycle. `bpm` allows interpolating to animate at 60 fps.
    void setPlayhead (double positionBeats, bool isPlaying, double bpm);

    EditCallbacks callbacks;
    std::function<void (model::NodeId)> onSelectionChanged;
    std::function<void (std::size_t trackIndex)> onActivateTrack;   // click on another track

    // Sets zoom and scroll so the whole graph fits in `visibleArea`
    // (in this component's coordinates), without zooming in past 100 %.
    void zoomToFit (juce::Rectangle<int> visibleArea);

    // Rounds the bottom corners (for the canvas inside a floating track window).
    void setBottomCornerRadius (float radius);

    model::NodeId getSelectedNode() const noexcept;
    void selectNode (model::NodeId node);
    void deleteSelection();

    // juce::Component
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    enum class Drag { None, MovingNode, Connecting, Panning };

    // Coordinates: the model uses pixels at 100 % zoom; the screen applies zoom
    // and scroll. `viewOrigin` is the model point at the top-left corner of the
    // area below the ruler.
    juce::Point<float> toScreen (double x, double y) const noexcept;
    juce::Point<float> toScreen (const model::Node& node) const noexcept;
    juce::Point<double> toModel (juce::Point<float> screen) const noexcept;

    const model::Node* nodeAt (juce::Point<float> screen) const noexcept;
    bool isOnRing (const model::Node& node, juce::Point<float> screen) const noexcept;
    model::EdgeId edgeAt (juce::Point<float> screen) const noexcept;

    void showNodeMenu (model::NodeId node);
    void setSelection (model::NodeId node, model::EdgeId edge);

    // Animation state of a note that sounded.
    struct Onset
    {
        double ms = 0.0;                    // when it started sounding
        std::vector<int> pitches;
        model::NodeId from = model::noNode;
    };

    // A track on the canvas, with its own animation (each track has its own cycle).
    struct Layer
    {
        model::TrackId id = 0;
        const model::Track* track = nullptr;
        std::shared_ptr<const engine::Schedule> schedule;
        std::vector<model::NodeId> unreachable;

        double animBeat = 0.0;                                     // position in its cycle
        std::unordered_map<model::NodeId, Onset> onsets;           // current cycle
        std::unordered_map<model::NodeId, Onset> previousOnsets;   // trail fading out
        double cycleStartMs = -1.0e9;
    };

    // Playback animation: called on every screen refresh.
    void onFrame();
    void detectOnsets (Layer& layer, double fromBeat, double toBeat, double nowMs);
    void startNewCycle (Layer& layer, double nowMs);
    bool isAnimating (double nowMs) const noexcept;

    float starRadius (const model::Node& node) const noexcept;
    const Layer* activeLayer() const noexcept;
    std::optional<std::size_t> backgroundTrackAt (juce::Point<float> screen) const;

    void paintSky (juce::Graphics& g) const;
    void paintAtlasGrid (juce::Graphics& g) const;
    void paintRuler (juce::Graphics& g) const;
    void paintLayer (juce::Graphics& g, const Layer& layer, bool isActive, double nowMs) const;
    void paintEdges (juce::Graphics& g, const Layer& layer, bool isActive) const;
    void paintLitEdges (juce::Graphics& g, const Layer& layer, double nowMs) const;
    void paintSparks (juce::Graphics& g, const Layer& layer) const;
    void paintNodes (juce::Graphics& g, const Layer& layer, bool isActive, double nowMs) const;
    void paintPlayhead (juce::Graphics& g) const;
    void paintConnectionPreview (juce::Graphics& g) const;
    void paintEmptyHint (juce::Graphics& g) const;

    static bool isSounding (const Layer& layer, model::NodeId node) noexcept;
    static bool isUnreachable (const Layer& layer, model::NodeId node) noexcept;

    std::vector<Layer> layers;
    std::size_t active = 0;
    const model::Track* track = nullptr;   // the active track (the one being edited)

    double playheadBeat = 0.0;      // global position
    double playheadMs = 0.0;        // when `playheadBeat` arrived
    double currentBpm = 120.0;
    bool playing = false;

    float bottomCornerRadius = 0.0f;

    juce::VBlankAttachment vblank { this, [this] { onFrame(); } };

    juce::Point<double> viewOrigin { -80.0, -60.0 };
    float zoom = 1.0f;

    model::NodeId selectedNode = model::noNode;
    model::EdgeId selectedEdge = 0;

    // Notes and duration inherited by new nodes (those of the last selected one).
    std::vector<int> lastPitches { 60 };
    double lastDuration = 1.0;

    Drag drag = Drag::None;
    model::NodeId dragNode = model::noNode;
    juce::Point<double> dragOffset;         // from the mouse to the node centre, in the model
    juce::Point<double> panStartOrigin;
    juce::Point<float> mousePosition;
    model::NodeId hoverTarget = model::noNode;
    model::NodeId hoverNode = model::noNode;   // under the mouse: shows its notes

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphView)
};

} // namespace chimedes::ui
