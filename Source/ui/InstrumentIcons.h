#pragma once

#include <array>
#include <cmath>
#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include "model/Instruments.h"

namespace chimedes::ui
{

// Stroke icons for the instruments, in the style of IconButton: drawn on an
// 18 × 18 grid and scaled to the box they are given.
namespace instrumentIcons
{
    // Extra icons that are not instruments.
    enum class Special { Sample, LoadSample };

    namespace detail
    {
        inline void wave (juce::Path& p, float x0, float x1, float centre, float amplitude, float cycles, float phase = 0.0f)
        {
            constexpr int steps = 48;
            for (int i = 0; i <= steps; ++i)
            {
                const auto t = static_cast<float> (i) / steps;
                const auto x = x0 + t * (x1 - x0);
                const auto y = centre - amplitude * std::sin (juce::MathConstants<float>::twoPi * cycles * t + phase);
                if (i == 0)
                    p.startNewSubPath (x, y);
                else
                    p.lineTo (x, y);
            }
        }

        inline void line (juce::Path& p, float x0, float y0, float x1, float y1)
        {
            p.startNewSubPath (x0, y0);
            p.lineTo (x1, y1);
        }

        // Stroke `stroke` and fill `fill`, both in grid units, inside `box`.
        inline void draw (juce::Graphics& g, juce::Rectangle<float> box, const juce::Path& stroke, const juce::Path& fill, float thickness)
        {
            const auto scale = std::min (box.getWidth(), box.getHeight()) / 18.0f;
            const auto transform = juce::AffineTransform::scale (scale)
                                       .translated (box.getCentreX() - 9.0f * scale, box.getCentreY() - 9.0f * scale);

            g.strokePath (stroke, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), transform);
            g.fillPath (fill, transform);
        }
    }

    // Draws the icon of a built-in instrument in the current colour.
    inline void draw (juce::Graphics& g, juce::Rectangle<float> box, model::InstrumentId id, float thickness = 1.5f)
    {
        namespace ids = model::instruments;
        using detail::line;
        using detail::wave;

        juce::Path s, f;   // stroked and filled parts

        switch (id)
        {
            case ids::piano:
            {
                s.addRoundedRectangle (1.5f, 3.5f, 15.0f, 11.0f, 2.0f);
                for (const auto x : { 5.25f, 9.0f, 12.75f })
                    line (s, x, 9.5f, x, 14.5f);
                for (const auto x : { 5.25f, 12.75f })
                    f.addRoundedRectangle (x - 1.1f, 3.5f, 2.2f, 6.0f, 0.6f);
                break;
            }

            case ids::electricPiano:
            {
                s.addRoundedRectangle (1.5f, 9.0f, 15.0f, 6.5f, 1.5f);
                for (const auto x : { 5.25f, 9.0f, 12.75f })
                    line (s, x, 12.0f, x, 15.5f);
                wave (s, 2.5f, 15.5f, 4.5f, 2.0f, 1.5f);
                break;
            }

            case ids::organ:
            {
                constexpr std::array<float, 5> heights { 8.0f, 11.0f, 14.0f, 11.0f, 8.0f };
                for (std::size_t i = 0; i < heights.size(); ++i)
                {
                    const auto x = 1.8f + 3.2f * static_cast<float> (i);
                    s.addRoundedRectangle (x, 16.0f - heights[i], 2.2f, heights[i], 1.1f);
                }
                break;
            }

            case ids::harpsichord:
            {
                // Seen from above: the straight side, the keyboard and the curved tail.
                s.startNewSubPath (4.0f, 16.0f);
                s.lineTo (14.0f, 16.0f);
                s.lineTo (14.0f, 12.0f);
                s.quadraticTo (14.0f, 5.0f, 7.0f, 2.0f);
                s.lineTo (4.0f, 2.0f);
                s.closeSubPath();
                line (s, 4.0f, 13.5f, 14.0f, 13.5f);
                line (s, 7.0f, 13.5f, 6.0f, 4.0f);
                break;
            }

            case ids::musicBox:
            {
                s.addRoundedRectangle (1.5f, 7.0f, 12.0f, 9.0f, 1.5f);
                line (s, 1.5f, 10.0f, 13.5f, 10.0f);
                s.startNewSubPath (13.5f, 12.0f);
                s.lineTo (16.5f, 12.0f);
                s.lineTo (16.5f, 9.0f);
                // A note coming out of it.
                f.addEllipse (5.0f, 2.8f, 2.6f, 2.0f);
                line (s, 7.4f, 3.6f, 7.4f, 0.6f);
                line (s, 7.4f, 0.6f, 9.4f, 1.6f);
                break;
            }

            case ids::bell:
            {
                s.startNewSubPath (3.0f, 13.5f);
                s.quadraticTo (4.0f, 12.5f, 4.2f, 9.5f);
                s.quadraticTo (4.5f, 3.5f, 9.0f, 3.5f);
                s.quadraticTo (13.5f, 3.5f, 13.8f, 9.5f);
                s.quadraticTo (14.0f, 12.5f, 15.0f, 13.5f);
                s.closeSubPath();
                line (s, 9.0f, 3.5f, 9.0f, 1.8f);
                f.addEllipse (7.7f, 14.5f, 2.6f, 2.6f);
                break;
            }

            case ids::marimba:
            case ids::vibraphone:
            {
                const bool vibes = id == ids::vibraphone;
                const auto centre = vibes ? 7.0f : 9.0f;
                for (int i = 0; i < 5; ++i)
                {
                    const auto h = (vibes ? 10.0f : 13.0f) - 1.6f * static_cast<float> (i);
                    s.addRoundedRectangle (1.8f + 3.2f * static_cast<float> (i), centre - h / 2.0f, 2.2f, h, 1.1f);
                }
                if (vibes)
                    wave (s, 2.0f, 16.0f, 15.0f, 1.2f, 2.5f);
                break;
            }

            case ids::kalimba:
            {
                s.addRoundedRectangle (3.0f, 1.5f, 12.0f, 15.0f, 3.0f);
                constexpr std::array<float, 5> lengths { 4.0f, 5.5f, 7.0f, 5.5f, 4.0f };
                for (std::size_t i = 0; i < lengths.size(); ++i)
                {
                    const auto x = 6.0f + 1.5f * static_cast<float> (i);
                    line (s, x, 3.8f, x, 3.8f + lengths[i]);
                }
                s.addEllipse (7.6f, 12.0f, 2.8f, 2.8f);
                break;
            }

            case ids::steelDrum:
            {
                s.addEllipse (1.5f, 3.0f, 15.0f, 7.0f);
                s.startNewSubPath (1.5f, 6.5f);
                s.lineTo (1.5f, 12.0f);
                s.quadraticTo (9.0f, 17.5f, 16.5f, 12.0f);
                s.lineTo (16.5f, 6.5f);
                f.addEllipse (4.6f, 4.8f, 2.4f, 1.6f);
                f.addEllipse (10.6f, 4.4f, 2.4f, 1.6f);
                f.addEllipse (7.8f, 7.0f, 2.4f, 1.6f);
                break;
            }

            case ids::pluck:
            {
                // A guitar, tilted.
                s.addEllipse (2.0f, 7.5f, 9.0f, 9.0f);
                s.addEllipse (5.4f, 10.9f, 2.2f, 2.2f);
                line (s, 9.6f, 8.9f, 15.0f, 3.5f);
                line (s, 13.8f, 2.3f, 16.2f, 4.7f);
                break;
            }

            case ids::harp:
            {
                s.startNewSubPath (4.0f, 16.0f);
                s.lineTo (4.0f, 3.0f);
                s.quadraticTo (9.5f, 0.5f, 15.0f, 7.0f);
                s.lineTo (4.0f, 16.0f);
                line (s, 7.0f, 2.6f, 7.0f, 13.5f);
                line (s, 10.0f, 3.0f, 10.0f, 11.1f);
                line (s, 13.0f, 5.0f, 13.0f, 8.6f);
                break;
            }

            case ids::strings:
            {
                // A violin with its bow across.
                s.startNewSubPath (9.0f, 2.0f);
                s.cubicTo (12.5f, 2.0f, 12.5f, 6.0f, 11.2f, 7.5f);
                s.cubicTo (14.0f, 8.5f, 14.0f, 16.0f, 9.0f, 16.0f);
                s.cubicTo (4.0f, 16.0f, 4.0f, 8.5f, 6.8f, 7.5f);
                s.cubicTo (5.5f, 6.0f, 5.5f, 2.0f, 9.0f, 2.0f);
                s.closeSubPath();
                line (s, 2.0f, 15.5f, 16.0f, 2.5f);
                break;
            }

            case ids::flute:
            {
                juce::Path tube;
                tube.addRoundedRectangle (0.5f, 7.6f, 17.0f, 2.8f, 1.4f);
                tube.applyTransform (juce::AffineTransform::rotation (-0.55f, 9.0f, 9.0f));
                s.addPath (tube);
                for (const auto t : { -3.5f, -0.5f, 2.5f, 5.0f })
                {
                    const auto x = 9.0f + t * std::cos (0.55f);
                    const auto y = 9.0f - t * std::sin (0.55f);
                    f.addEllipse (x - 0.7f, y - 0.7f, 1.4f, 1.4f);
                }
                break;
            }

            case ids::brass:
            {
                // A trumpet: mouthpiece, valves, the loop and the bell.
                line (s, 1.5f, 8.0f, 11.0f, 8.0f);
                s.startNewSubPath (11.0f, 6.5f);
                s.lineTo (16.5f, 3.0f);
                s.lineTo (16.5f, 13.0f);
                s.lineTo (11.0f, 9.5f);
                s.closeSubPath();
                for (const auto x : { 5.0f, 7.2f, 9.4f })
                    line (s, x, 4.5f, x, 8.0f);
                s.addRoundedRectangle (4.0f, 8.0f, 6.5f, 5.0f, 2.2f);
                break;
            }

            case ids::choir:
            {
                s.addEllipse (2.8f, 4.2f, 4.0f, 4.0f);
                s.addEllipse (11.2f, 4.2f, 4.0f, 4.0f);
                s.addEllipse (6.8f, 2.5f, 4.4f, 4.4f);
                s.startNewSubPath (1.0f, 14.5f);
                s.quadraticTo (1.5f, 9.5f, 4.8f, 9.5f);
                s.startNewSubPath (17.0f, 14.5f);
                s.quadraticTo (16.5f, 9.5f, 13.2f, 9.5f);
                s.startNewSubPath (4.5f, 16.0f);
                s.quadraticTo (4.8f, 8.5f, 9.0f, 8.5f);
                s.quadraticTo (13.2f, 8.5f, 13.5f, 16.0f);
                break;
            }

            case ids::pad:
            {
                // Soft layered waves.
                wave (s, 1.5f, 16.5f, 7.0f, 2.5f, 1.0f);
                wave (s, 1.5f, 16.5f, 11.0f, 2.5f, 1.0f, 1.2f);
                break;
            }

            case ids::lead:
            {
                s.startNewSubPath (1.5f, 12.5f);
                s.lineTo (5.0f, 12.5f);
                s.lineTo (5.0f, 5.5f);
                s.lineTo (9.0f, 5.5f);
                s.lineTo (9.0f, 12.5f);
                s.lineTo (13.0f, 12.5f);
                s.lineTo (13.0f, 5.5f);
                s.lineTo (16.5f, 5.5f);
                break;
            }

            case ids::synthPluck:
            {
                // A sawtooth that dies away.
                s.startNewSubPath (1.5f, 14.0f);
                s.lineTo (1.5f, 3.0f);
                s.lineTo (6.5f, 14.0f);
                s.lineTo (6.5f, 7.5f);
                s.lineTo (11.0f, 14.0f);
                s.lineTo (11.0f, 10.5f);
                s.lineTo (15.0f, 14.0f);
                s.lineTo (16.5f, 14.0f);
                break;
            }

            case ids::bass:
            {
                // Bass clef.
                s.startNewSubPath (4.0f, 6.5f);
                s.cubicTo (4.0f, 2.5f, 13.0f, 2.0f, 13.0f, 7.5f);
                s.cubicTo (13.0f, 11.5f, 9.0f, 14.5f, 4.0f, 16.0f);
                f.addEllipse (3.0f, 5.4f, 3.2f, 3.2f);
                f.addEllipse (14.9f, 4.6f, 2.0f, 2.0f);
                f.addEllipse (14.9f, 9.0f, 2.0f, 2.0f);
                break;
            }

            case ids::subBass:
            {
                // A speaker cone.
                s.addEllipse (1.5f, 1.5f, 15.0f, 15.0f);
                s.addEllipse (5.5f, 5.5f, 7.0f, 7.0f);
                f.addEllipse (7.8f, 7.8f, 2.4f, 2.4f);
                break;
            }

            case ids::acidBass:
            {
                // A resonant low-pass filter: flat, a sharp peak and a steep fall.
                s.startNewSubPath (1.5f, 8.0f);
                s.lineTo (7.5f, 8.0f);
                s.cubicTo (10.0f, 8.0f, 10.5f, 2.0f, 12.0f, 2.0f);
                s.cubicTo (13.5f, 2.0f, 13.8f, 10.0f, 16.5f, 16.0f);
                line (s, 1.5f, 16.0f, 16.5f, 16.0f);
                break;
            }

            default:
            {
                // Unknown: a note.
                f.addEllipse (4.0f, 11.0f, 5.0f, 4.0f);
                line (s, 8.5f, 13.0f, 8.5f, 3.0f);
                line (s, 8.5f, 3.0f, 13.0f, 5.5f);
                break;
            }
        }

        detail::draw (g, box, s, f, thickness);
    }

    inline void draw (juce::Graphics& g, juce::Rectangle<float> box, Special icon, float thickness = 1.5f)
    {
        using detail::line;
        juce::Path s, f;

        if (icon == Special::Sample)
        {
            // A recorded waveform.
            constexpr std::array<float, 8> heights { 3.0f, 7.0f, 12.0f, 6.0f, 14.0f, 9.0f, 5.0f, 2.5f };
            for (std::size_t i = 0; i < heights.size(); ++i)
            {
                const auto x = 2.0f + 2.0f * static_cast<float> (i);
                line (s, x, 9.0f - heights[i] / 2.0f, x, 9.0f + heights[i] / 2.0f);
            }
        }
        else
        {
            line (s, 9.0f, 3.5f, 9.0f, 14.5f);
            line (s, 3.5f, 9.0f, 14.5f, 9.0f);
        }

        detail::draw (g, box, s, f, thickness);
    }
}

} // namespace chimedes::ui
