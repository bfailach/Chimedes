#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Theme.h"

namespace chimedes::ui
{

// Botón de acción dibujado como icono de trazo: aleatorio (dado), nuevo (hoja),
// abrir (carpeta), guardar (disquete), deshacer y rehacer (flecha curva). El nombre
// aparece como tooltip.
class IconButton : public juce::Button
{
public:
    enum class Icon { Random, New, Open, Save, Undo, Redo };

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

        // Los iconos se dibujan en una caja de 18 × 18 centrada.
        const auto box = juce::Rectangle<float> (18.0f, 18.0f).withCentre (bounds.getCentre());
        const auto x = box.getX();
        const auto y = box.getY();

        g.setColour (colour);

        switch (icon)
        {
            case Icon::Random:
            {
                // Un dado: cuadrado redondeado con la cara del cinco.
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
                // Una hoja con la esquina doblada y un signo más.
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

            case Icon::Open:
            {
                // Carpeta con la solapa arriba a la izquierda.
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
                // Disquete: cuerpo con esquina cortada, etiqueta abajo y obturador arriba.
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
                // Flecha curva hacia la izquierda; rehacer es su reflejo.
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
