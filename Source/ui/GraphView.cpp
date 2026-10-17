#include "GraphView.h"

#include <algorithm>
#include <cmath>

#include <juce_audio_basics/juce_audio_basics.h>

#include "model/Units.h"

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
    const juce::Colour start        { 0xff4cc38a };
    const juce::Colour end          { 0xffe06c75 };
    const juce::Colour unreachable  { 0xffe0a84f };
    const juce::Colour playhead     { 0xffffffff };
}

GraphView::GraphView()
{
    setOpaque (true);
}

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

juce::Point<float> GraphView::toScreen (const model::Node& node) const noexcept
{
    return { originX + static_cast<float> (node.x), originY + static_cast<float> (node.y) };
}

void GraphView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    paintRuler (g);

    if (track == nullptr)
        return;

    paintEdges (g);
    paintPlayhead (g);
    paintNodes (g);
}

void GraphView::paintRuler (juce::Graphics& g) const
{
    g.setColour (colours::ruler);
    g.fillRect (0, 0, getWidth(), rulerHeight);
    g.setFont (12.0f);

    const auto beatWidth = static_cast<float> (model::pixelsPerBeat);

    for (int beat = 0; originX + static_cast<float> (beat) * beatWidth < static_cast<float> (getWidth()); ++beat)
    {
        const auto x = originX + static_cast<float> (beat) * beatWidth;

        g.setColour (colours::gridLine);
        g.drawVerticalLine (juce::roundToInt (x), static_cast<float> (rulerHeight), static_cast<float> (getHeight()));

        g.setColour (colours::quietText);
        g.drawText (juce::String (beat), juce::roundToInt (x) + 4, 0, 40, rulerHeight, juce::Justification::centredLeft);
    }
}

void GraphView::paintEdges (juce::Graphics& g) const
{
    g.setColour (colours::edge);

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

        if (track->directed)
            g.drawArrow (line, 1.5f, 10.0f, 10.0f);
        else
            g.drawLine (line, 1.5f);
    }
}

void GraphView::paintNodes (juce::Graphics& g) const
{
    for (const auto& node : track->nodes)
    {
        const auto centre = toScreen (node);
        const auto bounds = juce::Rectangle<float> (nodeRadius * 2.0f, nodeRadius * 2.0f).withCentre (centre);

        g.setColour (isSounding (node.id) ? colours::sounding : colours::node);
        g.fillEllipse (bounds);

        auto border = colours::nodeBorder;
        auto thickness = 1.5f;

        if (isUnreachable (node.id))     { border = colours::unreachable; thickness = 3.0f; }
        else if (node.id == track->startNode) { border = colours::start; thickness = 3.0f; }
        else if (node.id == track->endNode)   { border = colours::end;   thickness = 3.0f; }

        g.setColour (border);
        g.drawEllipse (bounds, thickness);

        // Nombre de la nota o notas del acorde, bajo el nodo.
        juce::StringArray names;
        for (const auto pitch : node.pitches)
            names.add (juce::MidiMessage::getMidiNoteName (pitch, true, true, 4));

        g.setColour (colours::quietText);
        g.setFont (12.0f);
        g.drawText (names.joinIntoString (" "),
                    juce::Rectangle<float> (120.0f, 16.0f).withCentre (centre.translated (0.0f, nodeRadius + 12.0f)),
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

    const auto x = originX + static_cast<float> (start->x + model::beatsToPixels (playheadBeat));

    g.setColour (colours::playhead.withAlpha (playing ? 0.8f : 0.35f));
    g.drawLine (x, static_cast<float> (rulerHeight), x, static_cast<float> (getHeight()), 1.5f);
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
