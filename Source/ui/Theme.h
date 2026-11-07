#pragma once

#include <juce_graphics/juce_graphics.h>

// "Starlight" theme: a star atlas on soft black. The UI is almost monochrome
// (starlight white on greys); colour only appears when music plays, on the
// canvas, with one hue per note (see pitchColour).
namespace chimedes::ui::theme
{
    // Surfaces
    inline const juce::Colour background     { 0xff161618 };  // canvas: soft black
    inline const juce::Colour grid           { 0xff212125 };  // beat and atlas lines
    inline const juce::Colour panel          { 0xff1c1c1f };  // floating panels
    inline const juce::Colour panelOutline   { 0xff2a2a2f };
    inline const juce::Colour control        { 0xff242428 };  // buttons, drop-downs
    inline const juce::Colour controlHover   { 0xff2c2c31 };
    inline const juce::Colour controlPressed { 0xff36363c };
    inline const juce::Colour outline        { 0xff2a2a2f };
    inline const juce::Colour focus          { 0xff6c6c74 };

    // Accent: starlight white. Used sparingly (play, selection, fills).
    inline const juce::Colour accent         { 0xfff4f6fa };

    // Text
    inline const juce::Colour textPrimary    { 0xffedecef };
    inline const juce::Colour textSecondary  { 0xff8a8a93 };
    inline const juce::Colour textDisabled   { 0xff55555d };

    // Graph
    inline const juce::Colour star           { 0xffa9a9b2 };  // node at rest
    inline const juce::Colour edge           { 0xff3a3a42 };
    inline const juce::Colour edgeSelected   { accent };
    inline const juce::Colour node           { accent };      // control accent (keys, active buttons)
    inline const juce::Colour nodeStart      { 0xff7fd8a4 };  // start node ring
    inline const juce::Colour nodeEnd        { 0xffb7a6ff };  // end node ring
    inline const juce::Colour unreachable    { 0xffe8a35a };  // orange
    inline const juce::Colour playhead       { accent };

    // Messages
    inline const juce::Colour warning        { 0xffe8b45f };
    inline const juce::Colour error          { 0xffe8736a };

    inline constexpr float cornerRadius = 14.0f;
    inline constexpr float controlRadius = 8.0f;

    // Spectrum: each pitch class has its hue, in order around the colour wheel
    // (C red, D orange, E green, G blue, A violet, B magenta). A given C is
    // always the same colour in any octave.
    inline juce::Colour pitchColour (int midiPitch, float brightness = 1.0f)
    {
        const auto pitchClass = ((midiPitch % 12) + 12) % 12;
        const auto hue = static_cast<float> (pitchClass) / 12.0f;
        return juce::Colour::fromHSL (hue, 0.95f, 0.62f * brightness, 1.0f);
    }

    // Pastel version left in the trail after the note sounds.
    inline juce::Colour pitchTrail (int midiPitch)
    {
        return pitchColour (midiPitch).interpolatedWith (accent, 0.45f);
    }

    // Fonts bundled with the application (see Fonts.cpp): Sora for the
    // UI and DM Mono for figures (time, values).
    juce::Typeface::Ptr sansTypeface();
    juce::Typeface::Ptr monoTypeface();

    inline juce::Font sans (float height) { return juce::Font (juce::FontOptions (sansTypeface()).withHeight (height)); }
    inline juce::Font mono (float height) { return juce::Font (juce::FontOptions (monoTypeface()).withHeight (height)); }
}
