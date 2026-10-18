#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Theme.h"

namespace chimedes::ui
{

// Botón de transporte dibujado como icono: detener, reproducir o pausa.
// Activado (toggle), el icono se pinta en el color de los nodos.
class TransportButton : public juce::Button
{
public:
    enum class Icon { Stop, Play, Pause };

    TransportButton (const juce::String& name, Icon iconToUse)
        : juce::Button (name), icon (iconToUse)
    {
        setTooltip (name);
    }

    void paintButton (juce::Graphics& g, bool isMouseOver, bool isDown) override
    {
        const auto bounds = getLocalBounds().toFloat();

        if (isMouseOver || isDown)
        {
            g.setColour (isDown ? theme::controlPressed : theme::controlHover);
            g.fillRoundedRectangle (bounds.reduced (2.0f), theme::controlRadius);
        }

        auto colour = getToggleState() ? theme::node : theme::textPrimary;
        if (! isEnabled())
            colour = theme::textDisabled;

        g.setColour (colour);

        const auto size = 16.0f;
        const auto area = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());

        switch (icon)
        {
            case Icon::Stop:
                g.fillRoundedRectangle (area.reduced (1.0f), 2.0f);
                break;

            case Icon::Play:
            {
                juce::Path triangle;
                triangle.addTriangle (area.getX() + 2.0f, area.getY(),
                                      area.getX() + 2.0f, area.getBottom(),
                                      area.getRight(), area.getCentreY());
                g.fillPath (triangle);
                break;
            }

            case Icon::Pause:
            {
                const auto barWidth = 5.0f;
                g.fillRoundedRectangle (area.getX() + 2.0f, area.getY(), barWidth, size, 1.5f);
                g.fillRoundedRectangle (area.getRight() - 2.0f - barWidth, area.getY(), barWidth, size, 1.5f);
                break;
            }
        }
    }

private:
    Icon icon;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportButton)
};

} // namespace chimedes::ui
