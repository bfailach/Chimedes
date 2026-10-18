#pragma once

#include <juce_graphics/juce_graphics.h>

// Colores de la interfaz: grises neutros (sin tinte azul) y color solo en los nodos.
namespace chimedes::ui::theme
{
    // Superficies
    inline const juce::Colour background     { 0xff1e1e1e };  // lienzo
    inline const juce::Colour grid           { 0xff262626 };  // líneas de pulso
    inline const juce::Colour panel          { 0xff2a2a2a };  // paneles flotantes
    inline const juce::Colour control        { 0xff353535 };  // botones, desplegables
    inline const juce::Colour controlHover   { 0xff3f3f3f };
    inline const juce::Colour controlPressed { 0xff4a4a4a };
    inline const juce::Colour outline        { 0xff3c3c3c };
    inline const juce::Colour focus          { 0xff6a6a6a };

    // Texto
    inline const juce::Colour textPrimary    { 0xffe4e4e4 };
    inline const juce::Colour textSecondary  { 0xff9c9c9c };
    inline const juce::Colour textDisabled   { 0xff626262 };

    // Grafo
    inline const juce::Colour edge           { 0xff6e6e6e };
    inline const juce::Colour edgeSelected   { 0xffe4e4e4 };
    inline const juce::Colour node           { 0xffb5524c };  // rojo del diseño
    inline const juce::Colour nodeSounding   { 0xffff7566 };
    inline const juce::Colour nodeStart      { 0xff6cc28a };  // verde
    inline const juce::Colour nodeEnd        { 0xffb58cd9 };  // violeta
    inline const juce::Colour unreachable    { 0xffe09a3e };  // naranja
    inline const juce::Colour playhead       { 0xffe4e4e4 };

    // Mensajes
    inline const juce::Colour warning        { 0xffe0a84f };
    inline const juce::Colour error          { 0xffe06c63 };

    inline constexpr float cornerRadius = 8.0f;
    inline constexpr float controlRadius = 4.0f;
}
