#include "GraphView.h"

#include <algorithm>
#include <cmath>

#include "model/NoteNames.h"
#include "model/TrackEditing.h"
#include "model/Units.h"
#include "ui/Text.h"

namespace chimedes::ui
{

namespace colours
{
    const juce::Colour background   { 0xff15171c };
    const juce::Colour ruler        { 0xff1e2128 };
    const juce::Colour gridLine     { 0xff23262e };
    const juce::Colour quietText    { 0xff8a909c };
    const juce::Colour edge         { 0xff5a6070 };
    const juce::Colour node         { 0xff2b3040 };
    const juce::Colour nodeBorder   { 0xff6b7385 };
    const juce::Colour sounding     { 0xff4fa3ff };
    const juce::Colour selection    { 0xffffffff };
    const juce::Colour start        { 0xff4cc38a };
    const juce::Colour end          { 0xffe06c75 };
    const juce::Colour unreachable  { 0xffe0a84f };
    const juce::Colour playhead     { 0xffffffff };
}

namespace
{
    constexpr float minZoom = 0.25f;
    constexpr float maxZoom = 4.0f;
    constexpr float edgeHitDistance = 6.0f;

    // Fracción interior del radio que mueve el nodo; el anillo exterior conecta.
    constexpr float moveAreaFraction = 0.55f;

    enum MenuItem { setStart = 1, setEnd, deleteNode };
}

GraphView::GraphView()
{
    setOpaque (true);
    setWantsKeyboardFocus (true);  // las teclas que no maneja suben a la ventana
}

// ---------------------------------------------------------------------------
// Datos
// ---------------------------------------------------------------------------

void GraphView::setTrack (const model::Track* newTrack)
{
    track = newTrack;
    repaint();
}

void GraphView::setSchedule (std::shared_ptr<const engine::Schedule> newSchedule)
{
    schedule = std::move (newSchedule);
    repaint();
}

void GraphView::setUnreachable (std::vector<model::NodeId> nodes)
{
    unreachable = std::move (nodes);
    repaint();
}

void GraphView::setPlayhead (double beatInCycle, bool isPlaying)
{
    if (std::abs (beatInCycle - playheadBeat) > 1e-6 || isPlaying != playing)
    {
        playheadBeat = beatInCycle;
        playing = isPlaying;
        repaint();
    }
}

// ---------------------------------------------------------------------------
// Selección
// ---------------------------------------------------------------------------

model::NodeId GraphView::getSelectedNode() const noexcept
{
    // Tras deshacer, el nodo seleccionado puede haber dejado de existir.
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
// Coordenadas y búsqueda
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

const model::Node* GraphView::nodeAt (juce::Point<float> screen) const noexcept
{
    if (track == nullptr)
        return nullptr;

    // Al revés del orden de dibujo: el nodo de arriba gana.
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
// Ratón
// ---------------------------------------------------------------------------

void GraphView::mouseMove (const juce::MouseEvent& e)
{
    const auto* node = nodeAt (e.position);

    if (node == nullptr)
        setMouseCursor (juce::MouseCursor::NormalCursor);
    else if (isOnRing (*node, e.position))
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    else
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
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
            // x >= 0: nada antes del pulso 0 de la regla.
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

    // El nuevo nodo copia las notas y la duración del último nodo seleccionado.
    const auto pitches = lastPitches;
    const auto duration = lastDuration;

    model::NodeId created = model::noNode;
    callbacks.edit ([&] (model::Track& t) { created = model::addNode (t, x, y, pitches, duration); });
    setSelection (created, 0);
}

void GraphView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Zoom alrededor del cursor: el punto bajo el ratón se queda quieto.
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
    menu.addItem (setStart, text ("Marcar como inicio"), true, track->startNode == node);
    menu.addItem (setEnd, text ("Marcar como fin"), true, track->endNode == node);
    menu.addSeparator();
    menu.addItem (deleteNode, text ("Eliminar nodo"));

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
// Dibujo
// ---------------------------------------------------------------------------

void GraphView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    paintRuler (g);

    if (track == nullptr)
        return;

    if (track->nodes.empty())
        paintEmptyHint (g);

    paintEdges (g);
    paintConnectionPreview (g);
    paintPlayhead (g);
    paintNodes (g);
}

void GraphView::paintRuler (juce::Graphics& g) const
{
    g.setColour (colours::ruler);
    g.fillRect (0, 0, getWidth(), rulerHeight);
    g.setFont (12.0f);

    const auto beatWidth = static_cast<float> (model::pixelsPerBeat) * zoom;

    // Con poco zoom, una etiqueta cada 2, 4 u 8 pulsos para que no se amontonen.
    int labelEvery = 1;
    while (beatWidth * static_cast<float> (labelEvery) < 36.0f)
        labelEvery *= 2;

    const auto firstBeat = static_cast<int> (std::floor (viewOrigin.x / model::pixelsPerBeat));

    for (int beat = std::max (0, firstBeat);; ++beat)
    {
        const auto x = toScreen (beat * model::pixelsPerBeat, 0.0).x;
        if (x > static_cast<float> (getWidth()))
            break;

        g.setColour (colours::gridLine);
        g.drawVerticalLine (juce::roundToInt (x), static_cast<float> (rulerHeight), static_cast<float> (getHeight()));

        if (beat % labelEvery == 0)
        {
            g.setColour (colours::quietText);
            g.drawText (juce::String (beat), juce::roundToInt (x) + 4, 0, 40, rulerHeight, juce::Justification::centredLeft);
        }
    }
}

void GraphView::paintEdges (juce::Graphics& g) const
{
    for (const auto& e : track->edges)
    {
        const auto* from = track->findNode (e.from);
        const auto* to = track->findNode (e.to);
        if (from == nullptr || to == nullptr)
            continue;

        const auto a = toScreen (*from);
        const auto b = toScreen (*to);
        const auto length = a.getDistanceFrom (b);
        if (length <= 2.0f * nodeRadius)
            continue;

        // La línea va de borde a borde de los círculos, no de centro a centro.
        const auto direction = (b - a) / length;
        const juce::Line<float> line (a + direction * nodeRadius, b - direction * (nodeRadius + 2.0f));

        const bool selected = e.id == selectedEdge;
        g.setColour (selected ? colours::selection : colours::edge);
        const auto thickness = selected ? 2.5f : 1.5f;

        if (track->directed)
            g.drawArrow (line, thickness, 10.0f, 10.0f);
        else
            g.drawLine (line, thickness);
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

    const float dashes[] = { 6.0f, 4.0f };
    g.setColour (colours::sounding);
    g.drawDashedLine (juce::Line<float> (toScreen (*from), end), dashes, 2, 1.5f);
}

void GraphView::paintNodes (juce::Graphics& g) const
{
    const auto selected = getSelectedNode();

    for (const auto& node : track->nodes)
    {
        const auto centre = toScreen (node);
        const auto bounds = juce::Rectangle<float> (nodeRadius * 2.0f, nodeRadius * 2.0f).withCentre (centre);

        if (node.id == selected || node.id == hoverTarget)
        {
            g.setColour (colours::selection.withAlpha (0.25f));
            g.fillEllipse (bounds.expanded (6.0f));
        }

        g.setColour (isSounding (node.id) ? colours::sounding : colours::node);
        g.fillEllipse (bounds);

        auto border = colours::nodeBorder;
        auto thickness = 1.5f;

        if (isUnreachable (node.id))          { border = colours::unreachable; thickness = 3.0f; }
        else if (node.id == track->startNode) { border = colours::start;       thickness = 3.0f; }
        else if (node.id == track->endNode)   { border = colours::end;         thickness = 3.0f; }

        g.setColour (border);
        g.drawEllipse (bounds, thickness);

        g.setColour (colours::quietText);
        g.setFont (12.0f);
        g.drawText (juce::String (model::pitchNames (node.pitches)),
                    juce::Rectangle<float> (140.0f, 16.0f).withCentre (centre.translated (0.0f, nodeRadius + 12.0f)),
                    juce::Justification::centred);

        if (node.id == track->startNode || node.id == track->endNode)
        {
            g.setColour (node.id == track->startNode ? colours::start : colours::end);
            g.drawText (node.id == track->startNode ? "inicio" : "fin",
                        juce::Rectangle<float> (80.0f, 14.0f).withCentre (centre.translated (0.0f, -nodeRadius - 10.0f)),
                        juce::Justification::centred);
        }
    }
}

void GraphView::paintPlayhead (juce::Graphics& g) const
{
    // La línea vertical solo tiene sentido en el modo línea de tiempo.
    if (track->mode != model::PlaybackMode::Timeline || schedule == nullptr)
        return;

    const auto* start = track->findNode (track->startNode);
    if (start == nullptr)
        return;

    const auto x = toScreen (start->x + model::beatsToPixels (playheadBeat), 0.0).x;

    g.setColour (colours::playhead.withAlpha (playing ? 0.8f : 0.35f));
    g.drawLine (x, static_cast<float> (rulerHeight), x, static_cast<float> (getHeight()), 1.5f);
}

void GraphView::paintEmptyHint (juce::Graphics& g) const
{
    g.setColour (colours::quietText);
    g.setFont (15.0f);
    g.drawText (text ("Doble clic para crear un nodo · arrastra desde el borde de un nodo para conectarlo"),
                getLocalBounds().withTrimmedTop (rulerHeight), juce::Justification::centred);
}

bool GraphView::isSounding (model::NodeId node) const noexcept
{
    if (! playing || schedule == nullptr)
        return false;

    return std::any_of (schedule->events.begin(), schedule->events.end(), [this, node] (const engine::NoteEvent& e)
    {
        return e.node == node && playheadBeat >= e.startBeat && playheadBeat < e.startBeat + e.durationBeats;
    });
}

bool GraphView::isUnreachable (model::NodeId node) const noexcept
{
    return std::find (unreachable.begin(), unreachable.end(), node) != unreachable.end();
}

} // namespace chimedes::ui
