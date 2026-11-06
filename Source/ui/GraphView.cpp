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

    // Fracción interior del radio que mueve el nodo; el anillo exterior conecta.
    constexpr float moveAreaFraction = 0.55f;

    enum MenuItem { setStart = 1, setEnd, deleteNode };

    // Tiempos de la animación, en milisegundos.
    constexpr double flashMs = 1600.0;      // del color intenso al pastel del rastro
    constexpr double swellMs = 400.0;       // la estrella se hincha al sonar
    constexpr double rippleMs = 1000.0;     // cada onda
    constexpr double rippleDelayMs = 140.0; // separación entre ondas
    constexpr double trailFadeMs = 900.0;   // el rastro del ciclo anterior se apaga
    constexpr double maxSparkBeats = 1.0;   // la chispa recorre la arista en ≤ 1 pulso

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

    // Color mientras destella: del tono intenso al pastel.
    juce::Colour flashColour (const std::vector<int>& pitches, double ageMs)
    {
        return mainColour (pitches).interpolatedWith (trailColour (pitches), clamp01 (ageMs / flashMs));
    }

    // Generador determinista para el fondo de estrellas: cada celda siempre igual.
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

void GraphView::setPlayhead (double beatInCycle, bool isPlaying, double bpm)
{
    // Al arrancar, la nota en la posición actual también debe destellar.
    // (el audio puede ir ya un poco adelante cuando llega la primera posición).
    if (isPlaying && ! playing)
        animBeat = std::min (animBeat, beatInCycle) - 1.0e-6;

    if (std::abs (beatInCycle - playheadBeat) > 1e-9 || isPlaying != playing)
        playheadMs = nowMs();

    playheadBeat = beatInCycle;
    playing = isPlaying;
    currentBpm = bpm;
}

// ---------------------------------------------------------------------------
// Animación
// ---------------------------------------------------------------------------

void GraphView::onFrame()
{
    const auto now = nowMs();
    const auto cycle = schedule != nullptr ? schedule->cycleLengthBeats : 0.0;

    if (playing && cycle > 0.0)
    {
        // La posición llega a ~30 Hz desde el audio; entre medias se interpola con el
        // tempo, sin adelantarse más de dos llamadas (si el audio se detiene, se para).
        const auto elapsed = std::min (now - playheadMs, 70.0) / 60000.0 * currentBpm;
        auto target = playheadBeat + std::max (0.0, elapsed);
        if (target >= cycle)
            target -= cycle;

        if (target + cycle * 0.5 < animBeat)   // dio la vuelta: ciclo nuevo
        {
            detectOnsets (animBeat, cycle, now);
            startNewCycle (now);
            detectOnsets (-1.0e-6, target, now);
            animBeat = target;
        }
        else if (target > animBeat)
        {
            detectOnsets (animBeat, target, now);
            animBeat = target;
        }
    }
    else
    {
        // Detenido (posición en 0 tras haber sonado): el rastro se apaga.
        if (! playing && playheadBeat <= 0.0 && ! onsets.empty())
            startNewCycle (now);

        animBeat = playheadBeat;
    }

    if (playing || isAnimating (now))
        repaint();
}

void GraphView::detectOnsets (double fromBeat, double toBeat, double now)
{
    if (schedule == nullptr)
        return;

    for (const auto& e : schedule->events)
        if (e.startBeat > fromBeat && e.startBeat <= toBeat)
            onsets[e.node] = { now, e.pitches, e.from };
}

void GraphView::startNewCycle (double now)
{
    previousOnsets = std::move (onsets);
    onsets.clear();
    cycleStartMs = now;
}

bool GraphView::isAnimating (double now) const noexcept
{
    if (now - cycleStartMs < trailFadeMs && ! previousOnsets.empty())
        return true;

    return std::any_of (onsets.begin(), onsets.end(), [now] (const auto& entry)
    {
        return now - entry.second.ms < flashMs;
    });
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

    // Margen para los círculos y las etiquetas de inicio, fin y notas.
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

float GraphView::starRadius (const model::Node& node) const noexcept
{
    // El tamaño sigue a la intensidad y crece un poco con el zoom.
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

    if (track->nodes.empty())
        paintEmptyHint (g);

    paintEdges (g);
    paintLitEdges (g, now);
    paintSparks (g);
    paintConnectionPreview (g);
    paintPlayhead (g);
    paintNodes (g, now);
}

void GraphView::paintSky (juce::Graphics& g) const
{
    g.fillAll (colours::background);

    // Fondo de estrellas tenues con paralaje: se mueve al 30 % del desplazamiento.
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
    // Líneas curvas de un atlas celeste, ancladas al modelo: se mueven con el grafo.
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
    // La regla y las líneas de pulso solo tienen sentido cuando X es el tiempo.
    if (track == nullptr || track->mode != model::PlaybackMode::Timeline)
        return;

    g.setFont (theme::mono (10.5f));

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

        g.setColour (colours::grid);
        g.drawVerticalLine (juce::roundToInt (x), static_cast<float> (rulerHeight), static_cast<float> (getHeight()));
        g.setColour (colours::textDisabled);
        g.drawVerticalLine (juce::roundToInt (x), static_cast<float> (rulerHeight) - 6.0f, static_cast<float> (rulerHeight));

        if (beat % labelEvery == 0)
        {
            // El número queda centrado sobre su línea.
            g.setColour (colours::textDisabled);
            g.drawText (juce::String (beat), juce::Rectangle<float> (40.0f, static_cast<float> (rulerHeight) - 8.0f).withCentre ({ x, (static_cast<float> (rulerHeight) - 6.0f) * 0.5f }),
                        juce::Justification::centred);
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
        const auto ra = starRadius (*from) + 5.0f;
        const auto rb = starRadius (*to) + 5.0f;
        const auto length = a.getDistanceFrom (b);
        if (length <= ra + rb)
            continue;

        // La línea va de borde a borde de las estrellas, con un pequeño espacio.
        const auto direction = (b - a) / length;
        const juce::Line<float> line (a + direction * ra, b - direction * rb);

        const bool selected = e.id == selectedEdge;
        g.setColour (selected ? colours::edgeSelected : colours::edge);
        const auto thickness = selected ? 1.8f : 1.0f;

        if (track->directed)
            g.drawArrow (line, thickness, 7.0f, 7.0f);
        else
            g.drawLine (line, thickness);
    }
}

void GraphView::paintLitEdges (juce::Graphics& g, double now) const
{
    // Aristas ya recorridas: degradado del color de una nota al de la siguiente.
    auto paintSet = [&] (const std::unordered_map<model::NodeId, Onset>& set, float fade, bool skipRelit)
    {
        for (const auto& [id, onset] : set)
        {
            if (onset.from == model::noNode || (skipRelit && onsets.contains (id)))
                continue;

            const auto* from = track->findNode (onset.from);
            const auto* to = track->findNode (id);
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

    const auto fade = 1.0f - clamp01 ((now - cycleStartMs) / trailFadeMs);
    if (fade > 0.0f)
        paintSet (previousOnsets, fade, true);

    paintSet (onsets, 1.0f, false);
}

void GraphView::paintSparks (juce::Graphics& g) const
{
    // Chispa que viaja hacia la próxima nota, con una cola que recorre el espectro.
    if (! playing || schedule == nullptr)
        return;

    for (const auto& e : schedule->events)
    {
        if (e.from == model::noNode || onsets.contains (e.node) || e.startBeat < animBeat)
            continue;

        const auto parent = std::find_if (schedule->events.begin(), schedule->events.end(),
                                          [&e] (const engine::NoteEvent& p) { return p.node == e.from; });
        if (parent == schedule->events.end() || parent->startBeat >= e.startBeat)
            continue;

        const auto window = std::min (e.startBeat - parent->startBeat, maxSparkBeats);
        const auto progress = (animBeat - (e.startBeat - window)) / window;
        if (progress <= 0.0 || progress >= 1.0)
            continue;

        const auto* from = track->findNode (e.from);
        const auto* to = track->findNode (e.node);
        if (from == nullptr || to == nullptr)
            continue;

        const auto a = toScreen (*from);
        const auto b = toScreen (*to);
        const auto hueA = parent->pitches.empty() ? 0.0f : static_cast<float> (parent->pitches.front() % 12) / 12.0f;
        const auto hueB = e.pitches.empty() ? 0.0f : static_cast<float> (e.pitches.front() % 12) / 12.0f;

        // Línea que se va trazando detrás de la chispa.
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

void GraphView::paintNodes (juce::Graphics& g, double now) const
{
    const auto selected = getSelectedNode();
    const auto trailFade = 1.0f - clamp01 ((now - cycleStartMs) / trailFadeMs);

    for (const auto& node : track->nodes)
    {
        const auto centre = toScreen (node);
        const auto radius = starRadius (node);
        const bool isStart = node.id == track->startNode;
        const bool isEnd = node.id == track->endNode;
        const bool hasStatusRing = isUnreachable (node.id) || isStart || isEnd;

        const auto onset = onsets.find (node.id);
        const bool lit = onset != onsets.end();
        const auto age = lit ? now - onset->second.ms : 0.0;
        const auto& pitches = lit ? onset->second.pitches : node.pitches;

        // Zona para conectar (el anillo exterior) al pasar el ratón.
        if (node.id == hoverNode && drag == Drag::None)
        {
            g.setColour (colours::accent.withAlpha (0.08f));
            g.fillEllipse (juce::Rectangle<float> (nodeRadius * 2.0f, nodeRadius * 2.0f).withCentre (centre));
        }

        // Ondas de colores vecinos; en un acorde, la última toma el color de su otra nota.
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

        // Halo mientras la nota suena.
        if (isSounding (node.id))
        {
            const auto halo = radius + 11.0f;
            g.setColour ((lit ? mainColour (pitches) : colours::accent).withAlpha (0.35f));
            g.drawEllipse (juce::Rectangle<float> (2.0f * halo, 2.0f * halo).withCentre (centre), 1.0f);
        }

        // La estrella: gris en reposo, color de su nota al sonar, pastel en el rastro.
        auto fill = colours::star;
        auto drawRadius = radius;

        if (lit)
        {
            fill = flashColour (pitches, age);
            drawRadius += 3.5f * (1.0f - clamp01 (age / swellMs));
        }
        else if (const auto previous = previousOnsets.find (node.id); previous != previousOnsets.end() && trailFade > 0.0f)
        {
            fill = colours::star.interpolatedWith (trailColour (previous->second.pitches), trailFade);
        }

        g.setColour (fill);
        g.fillEllipse (juce::Rectangle<float> (2.0f * drawRadius, 2.0f * drawRadius).withCentre (centre));

        // Anillo de estado: inalcanzable, inicio o fin.
        if (hasStatusRing)
        {
            const auto ring = isUnreachable (node.id) ? colours::unreachable
                            : isStart                 ? colours::nodeStart
                                                      : colours::nodeEnd;
            const auto r = radius + 5.0f;
            g.setColour (ring);
            g.drawEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (centre), 1.5f);
        }

        // Selección o destino al conectar: contorno blanco estelar por fuera de todo.
        if (node.id == selected || node.id == hoverTarget)
        {
            const auto r = radius + (hasStatusRing ? 10.0f : 6.0f);
            g.setColour (colours::accent);
            g.drawEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (centre), 1.2f);
        }

        // Nombre de las notas arriba a la derecha: tenue, más claro al seleccionar o
        // al pasar el ratón, y en el color de la nota mientras destella.
        auto labelColour = colours::textDisabled.brighter (0.25f);
        if (node.id == hoverNode || node.id == selected)
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
            g.drawText (isStart ? "inicio" : "fin",
                        juce::Rectangle<float> (80.0f, 14.0f).withCentre (centre.translated (0.0f, radius + 16.0f)),
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

    const auto x = toScreen (start->x + model::beatsToPixels (animBeat), 0.0).x;

    g.setColour (colours::playhead.withAlpha (playing ? 0.55f : 0.25f));
    g.drawLine (x, static_cast<float> (rulerHeight), x, static_cast<float> (getHeight()), 1.0f);
}

void GraphView::paintEmptyHint (juce::Graphics& g) const
{
    g.setColour (colours::textSecondary);
    g.setFont (theme::sans (15.0f));
    g.drawText (text ("Doble clic para crear una estrella · arrastra desde su borde para conectarla"),
                getLocalBounds().withTrimmedTop (rulerHeight), juce::Justification::centred);
}

bool GraphView::isSounding (model::NodeId node) const noexcept
{
    if (! playing || schedule == nullptr)
        return false;

    return std::any_of (schedule->events.begin(), schedule->events.end(), [this, node] (const engine::NoteEvent& e)
    {
        return e.node == node && animBeat >= e.startBeat && animBeat < e.startBeat + e.durationBeats;
    });
}

bool GraphView::isUnreachable (model::NodeId node) const noexcept
{
    return std::find (unreachable.begin(), unreachable.end(), node) != unreachable.end();
}

} // namespace chimedes::ui
