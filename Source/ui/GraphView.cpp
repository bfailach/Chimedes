#include "GraphView.h"

#include <algorithm>
#include <cmath>

#include "model/NoteNames.h"
#include "model/TrackEditing.h"
#include "model/Units.h"
#include "ui/Text.h"
#include "ui/Theme.h"

namespace chimedes::ui
{

namespace colours = theme;

namespace
{
    constexpr float minZoom = 0.25f;
    constexpr float maxZoom = 4.0f;
    constexpr float edgeHitDistance = 6.0f;

    // Inner fraction of the radius that moves the node; the outer ring connects.
    constexpr float moveAreaFraction = 0.55f;

    enum MenuItem { setStart = 1, setEnd, deleteNode };

    // Animation timings, in milliseconds.
    constexpr double flashMs = 1600.0;      // from the vivid colour to the trail's pastel
    constexpr double swellMs = 400.0;       // the star swells when it sounds
    constexpr double rippleMs = 1000.0;     // each ripple
    constexpr double rippleDelayMs = 140.0; // gap between ripples
    constexpr double trailFadeMs = 900.0;   // the previous cycle's trail fades out
    constexpr double maxSparkBeats = 1.0;   // the spark crosses the edge in ≤ 1 beat

    double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    float clamp01 (double v) { return static_cast<float> (std::clamp (v, 0.0, 1.0)); }

    juce::Colour mainColour (const std::vector<int>& pitches)
    {
        return pitches.empty() ? theme::accent : theme::pitchColour (pitches.front());
    }

    juce::Colour trailColour (const std::vector<int>& pitches)
    {
        return pitches.empty() ? theme::accent : theme::pitchTrail (pitches.front());
    }

    // Colour while flashing: from the vivid hue to the pastel.
    juce::Colour flashColour (const std::vector<int>& pitches, double ageMs)
    {
        return mainColour (pitches).interpolatedWith (trailColour (pitches), clamp01 (ageMs / flashMs));
    }

    // Deterministic generator for the star background: each cell always the same.
    juce::uint32 hashCell (int x, int y)
    {
        auto h = static_cast<juce::uint32> (x) * 73856093u ^ static_cast<juce::uint32> (y) * 19349663u;
        h ^= h >> 13;  h *= 0x5bd1e995u;  h ^= h >> 15;
        return h;
    }
}

GraphView::GraphView()
{
    setOpaque (true);
    setWantsKeyboardFocus (true);  // keys it does not handle go up to the window
}

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------

void GraphView::setTracks (std::vector<TrackView> tracks, std::size_t activeIndex)
{
    // Keeps each track's animation (by id) even if its content changes.
    std::vector<Layer> next;
    next.reserve (tracks.size());

    for (auto& view : tracks)
    {
        Layer layer;
        if (view.track != nullptr)
        {
            const auto previous = std::find_if (layers.begin(), layers.end(),
                                                [&view] (const Layer& l) { return l.id == view.track->id; });
            if (previous != layers.end())
                layer = std::move (*previous);
            layer.id = view.track->id;
        }

        layer.track = view.track;
        layer.schedule = std::move (view.schedule);
        layer.unreachable = std::move (view.unreachable);
        next.push_back (std::move (layer));
    }

    layers = std::move (next);
    active = layers.empty() ? 0 : std::min (activeIndex, layers.size() - 1);
    track = layers.empty() ? nullptr : layers[active].track;
    repaint();
}

const GraphView::Layer* GraphView::activeLayer() const noexcept
{
    return active < layers.size() ? &layers[active] : nullptr;
}

void GraphView::setPlayhead (double positionBeats, bool isPlaying, double bpm)
{
    // On start, the note at the current position must flash too
    // (audio may already be a little ahead when the first position arrives).
    if (isPlaying && ! playing)
        for (auto& layer : layers)
            if (layer.schedule != nullptr && layer.schedule->cycleLengthBeats > 0.0)
                layer.animBeat = std::min (layer.animBeat, std::fmod (positionBeats, layer.schedule->cycleLengthBeats)) - 1.0e-6;

    if (std::abs (positionBeats - playheadBeat) > 1e-9 || isPlaying != playing)
        playheadMs = nowMs();

    playheadBeat = positionBeats;
    playing = isPlaying;
    currentBpm = bpm;
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------

void GraphView::onFrame()
{
    const auto now = nowMs();

    // The position arrives at ~30 Hz from audio; in between it is interpolated with
    // the tempo, never more than two calls ahead (if audio stops, so does this).
    auto position = playheadBeat;
    if (playing)
        position += std::min (now - playheadMs, 70.0) / 60000.0 * currentBpm;

    for (auto& layer : layers)
    {
        const auto cycle = layer.schedule != nullptr ? layer.schedule->cycleLengthBeats : 0.0;

        if (playing && cycle > 0.0)
        {
            const auto target = std::fmod (position, cycle);

            if (target + cycle * 0.5 < layer.animBeat)   // wrapped around: new cycle
            {
                detectOnsets (layer, layer.animBeat, cycle, now);
                startNewCycle (layer, now);
                detectOnsets (layer, -1.0e-6, target, now);
                layer.animBeat = target;
            }
            else if (target > layer.animBeat)
            {
                detectOnsets (layer, layer.animBeat, target, now);
                layer.animBeat = target;
            }
        }
        else
        {
            // Stopped (position at 0 after playing): the trail fades out.
            if (! playing && playheadBeat <= 0.0 && ! layer.onsets.empty())
                startNewCycle (layer, now);

            layer.animBeat = cycle > 0.0 ? std::fmod (playheadBeat, cycle) : 0.0;
        }
    }

    if (playing || isAnimating (now))
        repaint();
}

void GraphView::detectOnsets (Layer& layer, double fromBeat, double toBeat, double now)
{
    if (layer.schedule == nullptr)
        return;

    for (const auto& e : layer.schedule->events)
        if (e.startBeat > fromBeat && e.startBeat <= toBeat)
            layer.onsets[e.node] = { now, e.pitches, e.from };
}

void GraphView::startNewCycle (Layer& layer, double now)
{
    layer.previousOnsets = std::move (layer.onsets);
    layer.onsets.clear();
    layer.cycleStartMs = now;
}

bool GraphView::isAnimating (double now) const noexcept
{
    return std::any_of (layers.begin(), layers.end(), [now] (const Layer& layer)
    {
        if (now - layer.cycleStartMs < trailFadeMs && ! layer.previousOnsets.empty())
            return true;

        return std::any_of (layer.onsets.begin(), layer.onsets.end(), [now] (const auto& entry)
        {
            return now - entry.second.ms < flashMs;
        });
    });
}

model::NodeId GraphView::getSelectedNode() const noexcept
{
    // After an undo, the selected node may no longer exist.
    return track != nullptr && track->findNode (selectedNode) != nullptr ? selectedNode : model::noNode;
}

void GraphView::selectNode (model::NodeId node)
{
    setSelection (node, 0);
}

void GraphView::setSelection (model::NodeId node, model::EdgeId edgeId)
{
    const bool changed = node != selectedNode || edgeId != selectedEdge;
    selectedNode = node;
    selectedEdge = edgeId;

    if (const auto* n = track != nullptr ? track->findNode (node) : nullptr)
    {
        lastPitches = n->pitches;
        lastDuration = n->durationBeats;
    }

    if (changed && onSelectionChanged)
        onSelectionChanged (node);

    repaint();
}

void GraphView::zoomToFit (juce::Rectangle<int> visibleArea)
{
    if (track == nullptr || track->nodes.empty() || visibleArea.isEmpty())
        return;

    auto minX = track->nodes.front().x, maxX = minX;
    auto minY = track->nodes.front().y, maxY = minY;

    for (const auto& n : track->nodes)
    {
        minX = std::min (minX, n.x);  maxX = std::max (maxX, n.x);
        minY = std::min (minY, n.y);  maxY = std::max (maxY, n.y);
    }

    // Margin for the circles and the start, end and note labels.
    constexpr double margin = 70.0;
    const auto width = (maxX - minX) + 2.0 * margin;
    const auto height = (maxY - minY) + 2.0 * margin;

    const auto area = visibleArea.withTrimmedTop (rulerHeight).toDouble();
    zoom = static_cast<float> (std::clamp (std::min (area.getWidth() / width, area.getHeight() / height),
                                           static_cast<double> (minZoom), 1.0));

    const auto centreX = (minX + maxX) * 0.5;
    const auto centreY = (minY + maxY) * 0.5;
    viewOrigin = { centreX - area.getCentreX() / zoom,
                   centreY - (area.getCentreY() - rulerHeight) / zoom };
    repaint();
}

void GraphView::deleteSelection()
{
    if (track == nullptr || ! callbacks.edit)
        return;

    if (const auto node = getSelectedNode(); node != model::noNode)
    {
        callbacks.edit ([node] (model::Track& t) { model::removeNode (t, node); });
        setSelection (model::noNode, 0);
    }
    else if (selectedEdge != 0)
    {
        const auto edgeId = selectedEdge;
        callbacks.edit ([edgeId] (model::Track& t) { model::removeEdge (t, edgeId); });
        setSelection (model::noNode, 0);
    }
}

// ---------------------------------------------------------------------------
// Coordinates and lookup
// ---------------------------------------------------------------------------

juce::Point<float> GraphView::toScreen (double x, double y) const noexcept
{
    return { static_cast<float> ((x - viewOrigin.x) * zoom),
             static_cast<float> (rulerHeight + (y - viewOrigin.y) * zoom) };
}

juce::Point<float> GraphView::toScreen (const model::Node& node) const noexcept
{
    return toScreen (node.x, node.y);
}

juce::Point<double> GraphView::toModel (juce::Point<float> screen) const noexcept
{
    return { viewOrigin.x + screen.x / zoom,
             viewOrigin.y + (screen.y - static_cast<float> (rulerHeight)) / zoom };
}

std::optional<std::size_t> GraphView::backgroundTrackAt (juce::Point<float> screen) const
{
    // Only the star's centre (not the ring): so it is hard to activate by accident.
    for (std::size_t i = layers.size(); i-- > 0;)
        if (i != active && layers[i].track != nullptr)
            for (const auto& node : layers[i].track->nodes)
                if (toScreen (node).getDistanceFrom (screen) <= starRadius (node) + 5.0f)
                    return i;

    return std::nullopt;
}

const model::Node* GraphView::nodeAt (juce::Point<float> screen) const noexcept
{
    if (track == nullptr)
        return nullptr;

    // Reverse drawing order: the node on top wins.
    for (auto it = track->nodes.rbegin(); it != track->nodes.rend(); ++it)
        if (toScreen (*it).getDistanceFrom (screen) <= nodeRadius + 4.0f)
            return &*it;

    return nullptr;
}

bool GraphView::isOnRing (const model::Node& node, juce::Point<float> screen) const noexcept
{
    return toScreen (node).getDistanceFrom (screen) > nodeRadius * moveAreaFraction;
}

model::EdgeId GraphView::edgeAt (juce::Point<float> screen) const noexcept
{
    if (track == nullptr)
        return 0;

    for (const auto& e : track->edges)
    {
        const auto* from = track->findNode (e.from);
        const auto* to = track->findNode (e.to);
        if (from == nullptr || to == nullptr)
            continue;

        juce::Point<float> nearest;
        const juce::Line<float> line (toScreen (*from), toScreen (*to));
        if (line.getDistanceFromPoint (screen, nearest) <= edgeHitDistance)
            return e.id;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

void GraphView::mouseMove (const juce::MouseEvent& e)
{
    const auto* node = nodeAt (e.position);

    if (const auto id = node != nullptr ? node->id : model::noNode; id != hoverNode)
    {
        hoverNode = id;
        repaint();
    }

    if (node == nullptr)
        setMouseCursor (juce::MouseCursor::NormalCursor);
    else if (isOnRing (*node, e.position))
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    else
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void GraphView::mouseExit (const juce::MouseEvent&)
{
    hoverNode = model::noNode;
    repaint();
}

void GraphView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    mousePosition = e.position;

    if (track == nullptr)
        return;

    const auto* node = nodeAt (e.position);

    if (e.mods.isPopupMenu())
    {
        if (node != nullptr)
        {
            setSelection (node->id, 0);
            showNodeMenu (node->id);
        }
        return;
    }

    if (e.mods.isMiddleButtonDown() || node == nullptr)
    {
        if (const auto edgeId = edgeAt (e.position); edgeId != 0 && ! e.mods.isMiddleButtonDown())
        {
            setSelection (model::noNode, edgeId);
            return;
        }

        // Click on another track's star: that track becomes active.
        if (! e.mods.isMiddleButtonDown())
            if (const auto other = backgroundTrackAt (e.position); other.has_value() && onActivateTrack)
            {
                setSelection (model::noNode, 0);
                onActivateTrack (*other);
                return;
            }

        if (! e.mods.isMiddleButtonDown())
            setSelection (model::noNode, 0);

        drag = Drag::Panning;
        panStartOrigin = viewOrigin;
        return;
    }

    setSelection (node->id, 0);
    dragNode = node->id;

    if (isOnRing (*node, e.position))
    {
        drag = Drag::Connecting;
        hoverTarget = model::noNode;
    }
    else
    {
        drag = Drag::MovingNode;
        const auto m = toModel (e.position);
        dragOffset = { node->x - m.x, node->y - m.y };

        if (callbacks.beginGesture)
            callbacks.beginGesture();
    }
}

void GraphView::mouseDrag (const juce::MouseEvent& e)
{
    mousePosition = e.position;

    switch (drag)
    {
        case Drag::MovingNode:
        {
            const auto m = toModel (e.position);
            // x >= 0: nothing before beat 0 of the ruler.
            const auto x = std::max (0.0, m.x + dragOffset.x);
            const auto y = m.y + dragOffset.y;
            const auto id = dragNode;

            if (callbacks.updateGesture)
                callbacks.updateGesture ([id, x, y] (model::Track& t) { model::moveNode (t, id, x, y); });
            break;
        }

        case Drag::Connecting:
        {
            const auto* target = nodeAt (e.position);
            hoverTarget = target != nullptr && target->id != dragNode ? target->id : model::noNode;
            repaint();
            break;
        }

        case Drag::Panning:
        {
            const auto delta = e.position - e.mouseDownPosition;
            viewOrigin = { panStartOrigin.x - delta.x / zoom, panStartOrigin.y - delta.y / zoom };
            repaint();
            break;
        }

        case Drag::None:
            break;
    }
}

void GraphView::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::MovingNode && callbacks.endGesture)
        callbacks.endGesture();

    if (drag == Drag::Connecting && hoverTarget != model::noNode && callbacks.edit)
    {
        const auto from = dragNode;
        const auto to = hoverTarget;
        callbacks.edit ([from, to] (model::Track& t) { model::addEdge (t, from, to); });
    }

    drag = Drag::None;
    dragNode = model::noNode;
    hoverTarget = model::noNode;
    repaint();
}

void GraphView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (track == nullptr || ! callbacks.edit || nodeAt (e.position) != nullptr || e.mods.isPopupMenu())
        return;

    const auto m = toModel (e.position);
    const auto x = std::max (0.0, m.x);
    const auto y = m.y;

    // The new node copies the notes and duration of the last selected node.
    const auto pitches = lastPitches;
    const auto duration = lastDuration;

    model::NodeId created = model::noNode;
    callbacks.edit ([&] (model::Track& t) { created = model::addNode (t, x, y, pitches, duration); });
    setSelection (created, 0);
}

void GraphView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Zoom around the cursor: the point under the mouse stays put.
    const auto anchor = toModel (e.position);
    const auto factor = std::pow (1.15f, wheel.deltaY * 4.0f);
    zoom = std::clamp (zoom * factor, minZoom, maxZoom);

    viewOrigin = { anchor.x - e.position.x / zoom,
                   anchor.y - (e.position.y - static_cast<float> (rulerHeight)) / zoom };
    repaint();
}

void GraphView::showNodeMenu (model::NodeId node)
{
    juce::PopupMenu menu;
    menu.addItem (setStart, "Mark as start", true, track->startNode == node);
    menu.addItem (setEnd, "Mark as end", true, track->endNode == node);
    menu.addSeparator();
    menu.addItem (deleteNode, "Delete node");

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                        [safeThis = juce::Component::SafePointer<GraphView> (this), node] (int result)
    {
        if (safeThis == nullptr || ! safeThis->callbacks.edit)
            return;

        switch (result)
        {
            case setStart:   safeThis->callbacks.edit ([node] (model::Track& t) { t.startNode = node; }); break;
            case setEnd:     safeThis->callbacks.edit ([node] (model::Track& t) { t.endNode = node; });   break;
            case deleteNode: safeThis->selectNode (node); safeThis->deleteSelection();                   break;
            default: break;
        }
    });
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

float GraphView::starRadius (const model::Node& node) const noexcept
{
    // Size follows velocity and grows a little with zoom.
    const auto base = 4.5f + 5.5f * static_cast<float> (std::clamp (node.velocity, 1, 127)) / 127.0f;
    return base * juce::jlimit (0.75f, 1.4f, std::sqrt (zoom));
}

void GraphView::paint (juce::Graphics& g)
{
    const auto now = nowMs();

    paintSky (g);

    if (track != nullptr && track->mode == model::PlaybackMode::Algorithm)
        paintAtlasGrid (g);

    paintRuler (g);

    if (track == nullptr)
        return;

    // The other tracks, dimmed and underneath: visible and animated, but not editable.
    if (layers.size() > 1)
    {
        g.beginTransparencyLayer (0.38f);
        for (std::size_t i = 0; i < layers.size(); ++i)
            if (i != active && layers[i].track != nullptr)
                paintLayer (g, layers[i], false, now);
        g.endTransparencyLayer();
    }

    if (track->nodes.empty())
        paintEmptyHint (g);

    paintLayer (g, layers[active], true, now);
}

void GraphView::paintLayer (juce::Graphics& g, const Layer& layer, bool isActive, double now) const
{
    paintEdges (g, layer, isActive);
    paintLitEdges (g, layer, now);
    paintSparks (g, layer);

    if (isActive)
    {
        paintConnectionPreview (g);
        paintPlayhead (g);
    }

    paintNodes (g, layer, isActive, now);
}

void GraphView::paintSky (juce::Graphics& g) const
{
    g.fillAll (colours::background);

    // Background of faint stars with parallax: it moves at 30 % of the scroll.
    constexpr float cell = 220.0f;
    constexpr float parallax = 0.3f;
    const auto offsetX = static_cast<float> (viewOrigin.x) * zoom * parallax;
    const auto offsetY = static_cast<float> (viewOrigin.y) * zoom * parallax;

    const auto firstX = static_cast<int> (std::floor (offsetX / cell));
    const auto firstY = static_cast<int> (std::floor (offsetY / cell));
    const auto columns = static_cast<int> (std::ceil (static_cast<float> (getWidth()) / cell)) + 1;
    const auto rows = static_cast<int> (std::ceil (static_cast<float> (getHeight()) / cell)) + 1;

    for (int cy = firstY; cy <= firstY + rows; ++cy)
        for (int cx = firstX; cx <= firstX + columns; ++cx)
        {
            juce::Random random (static_cast<juce::int64> (hashCell (cx, cy)));

            for (int i = 0; i < 7; ++i)
            {
                const auto x = (static_cast<float> (cx) + random.nextFloat()) * cell - offsetX;
                const auto y = (static_cast<float> (cy) + random.nextFloat()) * cell - offsetY;
                const auto r = 0.3f + 0.8f * random.nextFloat();
                const auto alpha = 0.05f + 0.25f * random.nextFloat();

                g.setColour (juce::Colours::white.withAlpha (alpha));
                g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
            }
        }
}

void GraphView::paintAtlasGrid (juce::Graphics& g) const
{
    // Curved lines of a celestial atlas, anchored to the model: they move with the graph.
    constexpr double spacing = 320.0;
    const auto topLeft = toModel ({ 0.0f, static_cast<float> (rulerHeight) });
    const auto bottomRight = toModel ({ static_cast<float> (getWidth()), static_cast<float> (getHeight()) });
    const auto w = static_cast<float> (getWidth());
    const auto h = static_cast<float> (getHeight());
    const auto bow = 28.0f * zoom;

    g.setColour (colours::grid);

    for (auto y = std::floor (topLeft.y / spacing) * spacing; y <= bottomRight.y + spacing; y += spacing)
    {
        const auto sy = toScreen (0.0, y).y;
        juce::Path p;
        p.startNewSubPath (0.0f, sy);
        p.quadraticTo (w * 0.5f, sy - bow, w, sy);
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }

    for (auto x = std::floor (topLeft.x / spacing) * spacing; x <= bottomRight.x + spacing; x += spacing)
    {
        const auto sx = toScreen (x, 0.0).x;
        juce::Path p;
        p.startNewSubPath (sx, static_cast<float> (rulerHeight));
        p.quadraticTo (sx - bow * 0.6f, h * 0.5f, sx, h);
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }
}

void GraphView::paintRuler (juce::Graphics& g) const
{
    // The ruler and beat lines only make sense when X is time.
    if (track == nullptr || track->mode != model::PlaybackMode::Timeline)
        return;

    g.setFont (theme::mono (10.5f));

    const auto beatWidth = static_cast<float> (model::pixelsPerBeat) * zoom;

    // When zoomed out, a label every 2, 4 or 8 beats so they do not crowd.
    int labelEvery = 1;
    while (beatWidth * static_cast<float> (labelEvery) < 36.0f)
        labelEvery *= 2;

    const auto firstBeat = static_cast<int> (std::floor (viewOrigin.x / model::pixelsPerBeat));

    for (int beat = std::max (0, firstBeat);; ++beat)
    {
        const auto x = toScreen (beat * model::pixelsPerBeat, 0.0).x;
        if (x > static_cast<float> (getWidth()))
            break;

        g.setColour (colours::grid);
        g.drawVerticalLine (juce::roundToInt (x), static_cast<float> (rulerHeight), static_cast<float> (getHeight()));
        g.setColour (colours::textDisabled);
        g.drawVerticalLine (juce::roundToInt (x), static_cast<float> (rulerHeight) - 6.0f, static_cast<float> (rulerHeight));

        if (beat % labelEvery == 0)
        {
            // The number is centred over its line.
            g.setColour (colours::textDisabled);
            g.drawText (juce::String (beat), juce::Rectangle<float> (40.0f, static_cast<float> (rulerHeight) - 8.0f).withCentre ({ x, (static_cast<float> (rulerHeight) - 6.0f) * 0.5f }),
                        juce::Justification::centred);
        }
    }
}

void GraphView::paintEdges (juce::Graphics& g, const Layer& layer, bool isActive) const
{
    for (const auto& e : layer.track->edges)
    {
        const auto* from = layer.track->findNode (e.from);
        const auto* to = layer.track->findNode (e.to);
        if (from == nullptr || to == nullptr)
            continue;

        const auto a = toScreen (*from);
        const auto b = toScreen (*to);
        const auto ra = starRadius (*from) + 5.0f;
        const auto rb = starRadius (*to) + 5.0f;
        const auto length = a.getDistanceFrom (b);
        if (length <= ra + rb)
            continue;

        // The line goes from star edge to star edge, with a small gap.
        const auto direction = (b - a) / length;
        const juce::Line<float> line (a + direction * ra, b - direction * rb);

        const bool selected = isActive && e.id == selectedEdge;
        g.setColour (selected ? colours::edgeSelected : colours::edge);
        const auto thickness = selected ? 1.8f : 1.0f;

        if (layer.track->directed)
            g.drawArrow (line, thickness, 7.0f, 7.0f);
        else
            g.drawLine (line, thickness);
    }
}

void GraphView::paintLitEdges (juce::Graphics& g, const Layer& layer, double now) const
{
    // Edges already travelled: a gradient from one note's colour to the next.
    auto paintSet = [&] (const std::unordered_map<model::NodeId, Onset>& set, float fade, bool skipRelit)
    {
        for (const auto& [id, onset] : set)
        {
            if (onset.from == model::noNode || (skipRelit && layer.onsets.contains (id)))
                continue;

            const auto* from = layer.track->findNode (onset.from);
            const auto* to = layer.track->findNode (id);
            if (from == nullptr || to == nullptr)
                continue;

            const auto a = toScreen (*from);
            const auto b = toScreen (*to);
            const auto ra = starRadius (*from) + 5.0f;
            const auto rb = starRadius (*to) + 5.0f;
            const auto length = a.getDistanceFrom (b);
            if (length <= ra + rb)
                continue;

            const auto direction = (b - a) / length;
            const auto p0 = a + direction * ra;
            const auto p1 = b - direction * rb;

            const auto age = now - onset.ms;
            const auto fromPitches = set.contains (onset.from) ? set.at (onset.from).pitches : from->pitches;
            const auto c0 = flashColour (fromPitches, age);
            const auto c1 = flashColour (onset.pitches, age);
            const auto alpha = (1.0f - 0.35f * clamp01 (age / flashMs)) * fade;

            g.setGradientFill (juce::ColourGradient (c0.withAlpha (alpha), p0, c1.withAlpha (alpha), p1, false));
            g.drawLine (juce::Line<float> (p0, p1), 2.0f);
        }
    };

    const auto fade = 1.0f - clamp01 ((now - layer.cycleStartMs) / trailFadeMs);
    if (fade > 0.0f)
        paintSet (layer.previousOnsets, fade, true);

    paintSet (layer.onsets, 1.0f, false);
}

void GraphView::paintSparks (juce::Graphics& g, const Layer& layer) const
{
    // Spark travelling towards the next note, with a tail that runs through the spectrum.
    if (! playing || layer.schedule == nullptr)
        return;

    for (const auto& e : layer.schedule->events)
    {
        if (e.from == model::noNode || layer.onsets.contains (e.node) || e.startBeat < layer.animBeat)
            continue;

        const auto parent = std::find_if (layer.schedule->events.begin(), layer.schedule->events.end(),
                                          [&e] (const engine::NoteEvent& p) { return p.node == e.from; });
        if (parent == layer.schedule->events.end() || parent->startBeat >= e.startBeat)
            continue;

        const auto window = std::min (e.startBeat - parent->startBeat, maxSparkBeats);
        const auto progress = (layer.animBeat - (e.startBeat - window)) / window;
        if (progress <= 0.0 || progress >= 1.0)
            continue;

        const auto* from = layer.track->findNode (e.from);
        const auto* to = layer.track->findNode (e.node);
        if (from == nullptr || to == nullptr)
            continue;

        const auto a = toScreen (*from);
        const auto b = toScreen (*to);
        const auto hueA = parent->pitches.empty() ? 0.0f : static_cast<float> (parent->pitches.front() % 12) / 12.0f;
        const auto hueB = e.pitches.empty() ? 0.0f : static_cast<float> (e.pitches.front() % 12) / 12.0f;

        // Line being drawn behind the spark.
        const auto head = a + (b - a) * static_cast<float> (progress);
        g.setGradientFill (juce::ColourGradient (theme::pitchColour (parent->pitches.empty() ? 0 : parent->pitches.front()).withAlpha (0.8f), a,
                                                 theme::pitchColour (e.pitches.empty() ? 0 : e.pitches.front()).withAlpha (0.8f), b, false));
        g.drawLine (juce::Line<float> (a + (b - a).toFloat() * (starRadius (*from) + 5.0f) / a.getDistanceFrom (b), head), 2.0f);

        for (int q = 0; q < 5; ++q)
        {
            const auto t = std::max (0.0, progress - q * 0.05);
            const auto p = a + (b - a) * static_cast<float> (t);
            const auto hue = std::fmod (hueA + (hueB - hueA) * static_cast<float> (t) + static_cast<float> (q) * 0.07f + 1.0f, 1.0f);
            const auto r = 3.0f - static_cast<float> (q) * 0.45f;

            g.setColour (juce::Colour::fromHSL (hue, 0.95f, 0.66f, 1.0f - static_cast<float> (q) * 0.18f));
            g.fillEllipse (p.x - r, p.y - r, 2.0f * r, 2.0f * r);
        }
    }
}

void GraphView::paintConnectionPreview (juce::Graphics& g) const
{
    if (drag != Drag::Connecting)
        return;

    const auto* from = track->findNode (dragNode);
    if (from == nullptr)
        return;

    auto end = mousePosition;
    if (const auto* target = track->findNode (hoverTarget))
        end = toScreen (*target);

    const float dashes[] = { 4.0f, 5.0f };
    g.setColour (colours::accent.withAlpha (0.8f));
    g.drawDashedLine (juce::Line<float> (toScreen (*from), end), dashes, 2, 1.2f);
}

void GraphView::paintNodes (juce::Graphics& g, const Layer& layer, bool isActive, double now) const
{
    const auto selected = isActive ? getSelectedNode() : model::noNode;
    const auto trailFade = 1.0f - clamp01 ((now - layer.cycleStartMs) / trailFadeMs);

    for (const auto& node : layer.track->nodes)
    {
        const auto centre = toScreen (node);
        const auto radius = starRadius (node);
        const bool isStart = node.id == layer.track->startNode;
        const bool isEnd = node.id == layer.track->endNode;
        const bool hasStatusRing = (isActive && isUnreachable (layer, node.id)) || isStart || isEnd;

        const auto onset = layer.onsets.find (node.id);
        const bool lit = onset != layer.onsets.end();
        const auto age = lit ? now - onset->second.ms : 0.0;
        const auto& pitches = lit ? onset->second.pitches : node.pitches;

        // Connection zone (the outer ring) on hover.
        if (isActive && node.id == hoverNode && drag == Drag::None)
        {
            g.setColour (colours::accent.withAlpha (0.08f));
            g.fillEllipse (juce::Rectangle<float> (nodeRadius * 2.0f, nodeRadius * 2.0f).withCentre (centre));
        }

        // Ripples in neighbouring colours; in a chord, the last one takes its other note's colour.
        if (lit)
        {
            for (int q = 0; q < 3; ++q)
            {
                const auto d = age - q * rippleDelayMs;
                if (d < 0.0 || d > rippleMs)
                    continue;

                const auto progress = clamp01 (d / rippleMs);
                const auto colour = (q == 2 && pitches.size() > 1)
                                        ? theme::pitchColour (pitches.back())
                                        : mainColour (pitches).withRotatedHue (static_cast<float> (q) * 35.0f / 360.0f);
                const auto r = radius + 4.0f + progress * (24.0f + 10.0f * static_cast<float> (q)) * juce::jlimit (0.75f, 1.4f, std::sqrt (zoom));

                g.setColour (colour.withAlpha (0.85f * (1.0f - progress)));
                g.drawEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (centre), 1.5f);
            }
        }

        // Halo while the note sounds.
        if (isSounding (layer, node.id))
        {
            const auto halo = radius + 11.0f;
            g.setColour ((lit ? mainColour (pitches) : colours::accent).withAlpha (0.35f));
            g.drawEllipse (juce::Rectangle<float> (2.0f * halo, 2.0f * halo).withCentre (centre), 1.0f);
        }

        // The star: grey at rest, its note's colour when sounding, pastel in the trail.
        auto fill = colours::star;
        auto drawRadius = radius;

        if (lit)
        {
            fill = flashColour (pitches, age);
            drawRadius += 3.5f * (1.0f - clamp01 (age / swellMs));
        }
        else if (const auto previous = layer.previousOnsets.find (node.id); previous != layer.previousOnsets.end() && trailFade > 0.0f)
        {
            fill = colours::star.interpolatedWith (trailColour (previous->second.pitches), trailFade);
        }

        g.setColour (fill);
        g.fillEllipse (juce::Rectangle<float> (2.0f * drawRadius, 2.0f * drawRadius).withCentre (centre));

        // Status ring: unreachable, start or end.
        if (hasStatusRing)
        {
            const auto ring = (isActive && isUnreachable (layer, node.id)) ? colours::unreachable
                            : isStart                 ? colours::nodeStart
                                                      : colours::nodeEnd;
            const auto r = radius + 5.0f;
            g.setColour (ring);
            g.drawEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (centre), 1.5f);
        }

        // Selection or connection target: starlight-white outline outside everything.
        if (node.id == selected || (isActive && node.id == hoverTarget))
        {
            const auto r = radius + (hasStatusRing ? 10.0f : 6.0f);
            g.setColour (colours::accent);
            g.drawEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (centre), 1.2f);
        }

        // Note names at the top right: faint, brighter when selected or hovered,
        // and in the note's colour while it flashes.
        auto labelColour = colours::textDisabled.brighter (0.25f);
        if ((isActive && node.id == hoverNode) || node.id == selected)
            labelColour = colours::textPrimary;
        if (lit)
            labelColour = mainColour (pitches).interpolatedWith (labelColour, clamp01 (age / 2200.0));

        g.setColour (labelColour);
        g.setFont (theme::sans (12.0f));
        const auto labelOrigin = centre.translated (radius + 7.0f, -radius - 18.0f);
        g.drawText (juce::String (model::pitchNames (node.pitches)),
                    juce::Rectangle<float> (labelOrigin.x, labelOrigin.y, 160.0f, 16.0f),
                    juce::Justification::centredLeft);

        if (isStart || isEnd)
        {
            g.setColour (isStart ? colours::nodeStart : colours::nodeEnd);
            g.setFont (theme::sans (11.0f));
            g.drawText (isStart ? "start" : "end",
                        juce::Rectangle<float> (80.0f, 14.0f).withCentre (centre.translated (0.0f, radius + 16.0f)),
                        juce::Justification::centred);
        }
    }
}

void GraphView::paintPlayhead (juce::Graphics& g) const
{
    // The vertical line only makes sense in timeline mode.
    const auto* layer = activeLayer();
    if (layer == nullptr || track->mode != model::PlaybackMode::Timeline || layer->schedule == nullptr)
        return;

    const auto* start = track->findNode (track->startNode);
    if (start == nullptr)
        return;

    const auto x = toScreen (start->x + model::beatsToPixels (layer->animBeat), 0.0).x;

    g.setColour (colours::playhead.withAlpha (playing ? 0.55f : 0.25f));
    g.drawLine (x, static_cast<float> (rulerHeight), x, static_cast<float> (getHeight()), 1.0f);
}

void GraphView::paintEmptyHint (juce::Graphics& g) const
{
    g.setColour (colours::textSecondary);
    g.setFont (theme::sans (15.0f));
    g.drawText (text ("Double-click to create a star · drag from its edge to connect it"),
                getLocalBounds().withTrimmedTop (rulerHeight), juce::Justification::centred);
}

bool GraphView::isSounding (const Layer& layer, model::NodeId node) noexcept
{
    if (layer.schedule == nullptr)
        return false;

    // Only while playing: the flash takes care of the rest.
    return std::any_of (layer.schedule->events.begin(), layer.schedule->events.end(), [&layer, node] (const engine::NoteEvent& e)
    {
        return e.node == node && layer.animBeat >= e.startBeat && layer.animBeat < e.startBeat + e.durationBeats
            && layer.onsets.contains (node);
    });
}

bool GraphView::isUnreachable (const Layer& layer, model::NodeId node) noexcept
{
    return std::find (layer.unreachable.begin(), layer.unreachable.end(), node) != layer.unreachable.end();
}

} // namespace chimedes::ui
