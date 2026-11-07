#pragma once

#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Theme.h"

namespace chimedes::ui
{

// Action button drawn as a stroke icon: random (dice), options (sliders), new
// (sheet), open (folder), save (floppy disk), export (tray), undo and redo (curved
// arrow), add (plus), close (cross), expand (diagonal arrows) and delete (bin). The
// name is shown as a tooltip.
class IconButton : public juce::Button
{
public:
    enum class Icon { Random, Options, New, Open, Save, Export, Undo, Redo, Add, Close, Expand, Delete };

    IconButton (const juce::String& name, Icon iconToUse)
        : juce::Button (name), icon (iconToUse)
    {
        setTooltip (name);
    }

    void paintButton (juce::Graphics& g, bool isMouseOver, bool isDown) override
    {
        const auto bounds = getLocalBounds().toFloat();

        if (isEnabled() && (isMouseOver || isDown))
        {
            g.setColour (isDown ? theme::controlPressed : theme::controlHover);
            g.fillRoundedRectangle (bounds.reduced (2.0f), theme::controlRadius);
        }

        const auto colour = isEnabled() ? theme::textPrimary : theme::textDisabled;
        const juce::PathStrokeType stroke (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        // Icons are drawn in a centred 18 × 18 box.
        const auto box = juce::Rectangle<float> (18.0f, 18.0f).withCentre (bounds.getCentre());
        const auto x = box.getX();
        const auto y = box.getY();

        g.setColour (colour);

        switch (icon)
        {
            case Icon::Random:
            {
                // A die: rounded square showing the five.
                const auto face = box.reduced (1.5f);
                g.drawRoundedRectangle (face, 3.5f, 1.6f);

                const auto pip = [&] (float px, float py)
                {
                    g.fillEllipse (face.getX() + px * face.getWidth() - 1.4f,
                                   face.getY() + py * face.getHeight() - 1.4f, 2.8f, 2.8f);
                };
                pip (0.28f, 0.28f);  pip (0.72f, 0.28f);
                pip (0.5f, 0.5f);
                pip (0.28f, 0.72f);  pip (0.72f, 0.72f);
                break;
            }

            case Icon::New:
            {
                // A sheet with a folded corner and a plus sign.
                juce::Path page;
                page.startNewSubPath (x + 4.0f, y + 1.5f);
                page.lineTo (x + 11.0f, y + 1.5f);
                page.lineTo (x + 15.0f, y + 5.5f);
                page.lineTo (x + 15.0f, y + 16.5f);
                page.lineTo (x + 4.0f, y + 16.5f);
                page.closeSubPath();
                page.startNewSubPath (x + 11.0f, y + 1.5f);
                page.lineTo (x + 11.0f, y + 5.5f);
                page.lineTo (x + 15.0f, y + 5.5f);
                g.strokePath (page, stroke);

                juce::Path plus;
                plus.startNewSubPath (x + 9.5f, y + 8.5f);
                plus.lineTo (x + 9.5f, y + 13.5f);
                plus.startNewSubPath (x + 7.0f, y + 11.0f);
                plus.lineTo (x + 12.0f, y + 11.0f);
                g.strokePath (plus, stroke);
                break;
            }

            case Icon::Export:
            {
                // A tray with an arrow coming out upwards.
                juce::Path tray;
                tray.startNewSubPath (x + 2.0f, y + 10.5f);
                tray.lineTo (x + 2.0f, y + 16.0f);
                tray.lineTo (x + 16.0f, y + 16.0f);
                tray.lineTo (x + 16.0f, y + 10.5f);
                tray.startNewSubPath (x + 9.0f, y + 12.0f);
                tray.lineTo (x + 9.0f, y + 2.0f);
                tray.startNewSubPath (x + 5.0f, y + 6.0f);
                tray.lineTo (x + 9.0f, y + 2.0f);
                tray.lineTo (x + 13.0f, y + 6.0f);
                g.strokePath (tray, stroke);
                break;
            }

            case Icon::Options:
            {
                // Three sliders with their knobs at different heights.
                juce::Path lines;
                for (const auto lx : { 4.0f, 9.0f, 14.0f })
                {
                    lines.startNewSubPath (x + lx, y + 2.0f);
                    lines.lineTo (x + lx, y + 16.0f);
                }
                g.strokePath (lines, stroke);
                for (const auto& [kx, ky] : { std::pair { 4.0f, 11.5f }, std::pair { 9.0f, 5.5f }, std::pair { 14.0f, 9.5f } })
                {
                    g.setColour (theme::panel);
                    g.fillEllipse (x + kx - 2.6f, y + ky - 2.6f, 5.2f, 5.2f);
                    g.setColour (colour);
                    g.drawEllipse (x + kx - 2.6f, y + ky - 2.6f, 5.2f, 5.2f, 1.5f);
                }
                break;
            }

            case Icon::Close:
            {
                juce::Path cross;
                cross.startNewSubPath (x + 4.0f, y + 4.0f);
                cross.lineTo (x + 14.0f, y + 14.0f);
                cross.startNewSubPath (x + 14.0f, y + 4.0f);
                cross.lineTo (x + 4.0f, y + 14.0f);
                g.strokePath (cross, stroke);
                break;
            }

            case Icon::Expand:
            {
                // Two arrows pointing to opposite corners.
                juce::Path arrows;
                arrows.startNewSubPath (x + 10.5f, y + 2.5f);
                arrows.lineTo (x + 15.5f, y + 2.5f);
                arrows.lineTo (x + 15.5f, y + 7.5f);
                arrows.startNewSubPath (x + 15.5f, y + 2.5f);
                arrows.lineTo (x + 10.0f, y + 8.0f);
                arrows.startNewSubPath (x + 7.5f, y + 15.5f);
                arrows.lineTo (x + 2.5f, y + 15.5f);
                arrows.lineTo (x + 2.5f, y + 10.5f);
                arrows.startNewSubPath (x + 2.5f, y + 15.5f);
                arrows.lineTo (x + 8.0f, y + 10.0f);
                g.strokePath (arrows, stroke);
                break;
            }

            case Icon::Delete:
            {
                // A bin with a lid.
                juce::Path bin;
                bin.startNewSubPath (x + 2.5f, y + 4.5f);
                bin.lineTo (x + 15.5f, y + 4.5f);
                bin.startNewSubPath (x + 7.0f, y + 4.5f);
                bin.lineTo (x + 7.5f, y + 2.0f);
                bin.lineTo (x + 10.5f, y + 2.0f);
                bin.lineTo (x + 11.0f, y + 4.5f);
                bin.startNewSubPath (x + 4.0f, y + 4.5f);
                bin.lineTo (x + 5.0f, y + 16.0f);
                bin.lineTo (x + 13.0f, y + 16.0f);
                bin.lineTo (x + 14.0f, y + 4.5f);
                bin.startNewSubPath (x + 7.5f, y + 7.5f);
                bin.lineTo (x + 7.5f, y + 13.0f);
                bin.startNewSubPath (x + 10.5f, y + 7.5f);
                bin.lineTo (x + 10.5f, y + 13.0f);
                g.strokePath (bin, stroke);
                break;
            }

            case Icon::Add:
            {
                juce::Path plus;
                plus.startNewSubPath (x + 9.0f, y + 3.5f);
                plus.lineTo (x + 9.0f, y + 14.5f);
                plus.startNewSubPath (x + 3.5f, y + 9.0f);
                plus.lineTo (x + 14.5f, y + 9.0f);
                g.strokePath (plus, stroke);
                break;
            }

            case Icon::Open:
            {
                // A folder with its tab at the top left.
                juce::Path folder;
                folder.startNewSubPath (x + 1.5f, y + 4.0f);
                folder.lineTo (x + 6.5f, y + 4.0f);
                folder.lineTo (x + 8.5f, y + 6.0f);
                folder.lineTo (x + 16.5f, y + 6.0f);
                folder.lineTo (x + 16.5f, y + 15.0f);
                folder.lineTo (x + 1.5f, y + 15.0f);
                folder.closeSubPath();
                folder.startNewSubPath (x + 1.5f, y + 8.5f);
                folder.lineTo (x + 16.5f, y + 8.5f);
                g.strokePath (folder, stroke);
                break;
            }

            case Icon::Save:
            {
                // Floppy disk: body with a cut corner, label below and shutter above.
                juce::Path disk;
                disk.startNewSubPath (x + 2.0f, y + 2.0f);
                disk.lineTo (x + 13.0f, y + 2.0f);
                disk.lineTo (x + 16.0f, y + 5.0f);
                disk.lineTo (x + 16.0f, y + 16.0f);
                disk.lineTo (x + 2.0f, y + 16.0f);
                disk.closeSubPath();
                disk.startNewSubPath (x + 5.5f, y + 2.0f);
                disk.lineTo (x + 5.5f, y + 6.0f);
                disk.lineTo (x + 11.5f, y + 6.0f);
                disk.lineTo (x + 11.5f, y + 2.0f);
                disk.addRoundedRectangle (x + 5.0f, y + 10.0f, 8.0f, 6.0f, 1.0f);
                g.strokePath (disk, stroke);
                break;
            }

            case Icon::Undo:
            case Icon::Redo:
            {
                // Arrow curving to the left; redo is its mirror image.
                juce::Path arrow;
                arrow.startNewSubPath (x + 15.5f, y + 14.5f);
                arrow.cubicTo (x + 15.5f, y + 8.0f, x + 11.5f, y + 6.0f, x + 3.0f, y + 6.0f);
                arrow.startNewSubPath (x + 6.5f, y + 2.5f);
                arrow.lineTo (x + 3.0f, y + 6.0f);
                arrow.lineTo (x + 6.5f, y + 9.5f);

                if (icon == Icon::Redo)
                    arrow.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f, box.getCentreX(), box.getCentreY()));

                g.strokePath (arrow, stroke);
                break;
            }
        }
    }

private:
    Icon icon;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IconButton)
};

} // namespace chimedes::ui
