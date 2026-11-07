#pragma once

#include <cmath>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Theme.h"

namespace chimedes::ui
{

// The Chimedes mark: a starlight sparkle connected to one node. The line and
// the node carry the full spectrum; while music plays the rainbow slowly
// turns, and it holds still when playback stops.
// Geometry matches Resources/Logo/chimedes-logo.svg (a 600 x 600 box at 5, 45).
class LogoMark : public juce::Component
{
public:
    LogoMark()
    {
        setInterceptsMouseClicks (false, false);
        setTitle ("Chimedes");
    }

    // Called from the main timer (about 30 times a second).
    void tick (bool playing)
    {
        if (! playing)
            return;

        constexpr double turnSeconds = 14.0;
        constexpr double tickSeconds = 1.0 / 30.0;
        phase = std::fmod (phase + tickSeconds / turnSeconds, 1.0);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        constexpr float boxX = 5.0f, boxY = 45.0f, boxSize = 600.0f;
        const auto bounds = getLocalBounds().toFloat();
        const auto scale = std::min (bounds.getWidth(), bounds.getHeight()) / boxSize;
        const auto transform = juce::AffineTransform::translation (-boxX, -boxY)
                                   .scaled (scale)
                                   .translated (bounds.getCentreX() - boxSize * scale * 0.5f,
                                                bounds.getCentreY() - boxSize * scale * 0.5f);

        const juce::Point<float> starCentre { 260.0f, 380.0f }, node { 470.0f, 190.0f };

        // Rainbow: the full hue circle (red back to red, so it turns seamlessly)
        // along a line through the centre, rotated by the current phase.
        const juce::Point<float> centre { 338.0f, 338.0f };
        const auto angle = static_cast<float> (phase * juce::MathConstants<double>::twoPi);
        const auto offset = juce::Point<float> (std::cos (angle), std::sin (angle)) * 260.0f;
        juce::ColourGradient rainbow (hue (0.0f), (centre - offset).transformedBy (transform),
                                      hue (1.0f), (centre + offset).transformedBy (transform), false);
        for (int i = 1; i < 6; ++i)
            rainbow.addColour (i / 6.0, hue (i / 6.0f));

        juce::Path edge;
        edge.startNewSubPath (starCentre);
        edge.lineTo (node);

        juce::Path dot;
        dot.addEllipse (juce::Rectangle<float> (140.0f, 140.0f).withCentre (node));

        juce::Path star = sparkle (starCentre, 190.0f);

        // Scale the shapes first, so the line width scales with them.
        edge.applyTransform (transform);
        dot.applyTransform (transform);
        star.applyTransform (transform);

        g.setGradientFill (rainbow);
        g.strokePath (edge, juce::PathStrokeType (std::max (1.5f, 16.0f * scale),
                                                  juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillPath (dot);

        g.setColour (theme::accent);
        g.fillPath (star);
    }

private:
    static juce::Colour hue (float h)
    {
        return juce::Colour::fromHSL (h, 0.95f, 0.62f, 1.0f);
    }

    // Four-point sparkle with curved sides: tips at `radius`, curves pulled
    // towards the centre by control points at 14 % of the radius.
    static juce::Path sparkle (juce::Point<float> c, float radius)
    {
        const auto inner = radius * 0.14f;
        auto at = [c] (float r, int step)
        {
            const auto a = juce::degreesToRadians (-90.0f + 45.0f * static_cast<float> (step));
            return c + juce::Point<float> (std::cos (a), std::sin (a)) * r;
        };

        juce::Path p;
        p.startNewSubPath (at (radius, 0));
        for (int i = 0; i < 4; ++i)
            p.quadraticTo (at (inner, 2 * i + 1), at (radius, (2 * i + 2) % 8));
        p.closeSubPath();
        return p;
    }

    double phase = 0.875;  // resting position: a spread of hues across the mark

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LogoMark)
};

} // namespace chimedes::ui
