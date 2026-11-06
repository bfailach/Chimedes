#pragma once

#include <juce_graphics/juce_graphics.h>

// Tema "Starlight": un atlas estelar sobre negro suave. La interfaz es casi
// monocroma (blanco estelar sobre grises); el color aparece solo cuando suena la
// música, en el lienzo, con un tono por cada nota (ver pitchColour).
namespace chimedes::ui::theme
{
    // Superficies
    inline const juce::Colour background     { 0xff161618 };  // lienzo: negro suave
    inline const juce::Colour grid           { 0xff212125 };  // líneas de pulso y del atlas
    inline const juce::Colour panel          { 0xff1c1c1f };  // paneles flotantes
    inline const juce::Colour panelOutline   { 0xff2a2a2f };
    inline const juce::Colour control        { 0xff242428 };  // botones, desplegables
    inline const juce::Colour controlHover   { 0xff2c2c31 };
    inline const juce::Colour controlPressed { 0xff36363c };
    inline const juce::Colour outline        { 0xff2a2a2f };
    inline const juce::Colour focus          { 0xff6c6c74 };

    // Acento: blanco estelar. Se usa con moderación (reproducir, selección, rellenos).
    inline const juce::Colour accent         { 0xfff4f6fa };

    // Texto
    inline const juce::Colour textPrimary    { 0xffedecef };
    inline const juce::Colour textSecondary  { 0xff8a8a93 };
    inline const juce::Colour textDisabled   { 0xff55555d };

    // Grafo
    inline const juce::Colour star           { 0xffa9a9b2 };  // nodo en reposo
    inline const juce::Colour edge           { 0xff3a3a42 };
    inline const juce::Colour edgeSelected   { accent };
    inline const juce::Colour node           { accent };      // acento de controles (teclas, botones activos)
    inline const juce::Colour nodeStart      { 0xff7fd8a4 };  // anillo del nodo de inicio
    inline const juce::Colour nodeEnd        { 0xffb7a6ff };  // anillo del nodo de fin
    inline const juce::Colour unreachable    { 0xffe8a35a };  // naranja
    inline const juce::Colour playhead       { accent };

    // Mensajes
    inline const juce::Colour warning        { 0xffe8b45f };
    inline const juce::Colour error          { 0xffe8736a };

    inline constexpr float cornerRadius = 14.0f;
    inline constexpr float controlRadius = 8.0f;

    // Espectro: cada clase de nota tiene su tono, en orden por la rueda de color
    // (do rojo, re naranja, mi verde, sol azul, la violeta, si magenta). Un mismo
    // do es siempre del mismo color en cualquier octava.
    inline juce::Colour pitchColour (int midiPitch, float brightness = 1.0f)
    {
        const auto pitchClass = ((midiPitch % 12) + 12) % 12;
        const auto hue = static_cast<float> (pitchClass) / 12.0f;
        return juce::Colour::fromHSL (hue, 0.95f, 0.62f * brightness, 1.0f);
    }

    // Versión pastel que queda en el rastro después de que la nota suena.
    inline juce::Colour pitchTrail (int midiPitch)
    {
        return pitchColour (midiPitch).interpolatedWith (accent, 0.45f);
    }

    // Tipografías incluidas en la aplicación (ver Fonts.cpp): Space Grotesk para la
    // interfaz y JetBrains Mono para cifras (tiempo, valores).
    juce::Typeface::Ptr sansTypeface();
    juce::Typeface::Ptr monoTypeface();

    inline juce::Font sans (float height) { return juce::Font (juce::FontOptions (sansTypeface()).withHeight (height)); }
    inline juce::Font mono (float height) { return juce::Font (juce::FontOptions (monoTypeface()).withHeight (height)); }
}
