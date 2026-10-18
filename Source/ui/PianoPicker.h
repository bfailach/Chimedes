#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

namespace chimedes::ui
{

// Teclado de piano de dos octavas para elegir las notas de un nodo.
//   clic ............. esa nota sola
//   Mayús + clic ..... añade o quita la nota del acorde (siempre queda al menos una)
//   < > .............. cambia de octava
class PianoPicker : public juce::Component
{
public:
    PianoPicker();

    // Se llama con las notas nuevas, ordenadas de grave a agudo.
    std::function<void (std::vector<int>)> onChange;

    // Muestra estas notas. Si la más grave queda fuera de vista, mueve el teclado a ella.
    void setPitches (const std::vector<int>& pitches);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    static constexpr int whiteKeysShown = 14;  // dos octavas
    static constexpr int headerHeight = 24;

    int lowestPitch() const noexcept { return (baseOctave + 1) * 12; }
    int pitchAt (juce::Point<float> position) const;
    juce::Rectangle<float> keyBounds (int pitch) const;
    juce::Rectangle<float> keyboardArea() const;
    void setBaseOctave (int octave);
    void updateRangeLabel();

    static bool isBlack (int pitch) noexcept;

    std::vector<int> pitches;
    int baseOctave = 3;  // el teclado empieza en C3

    juce::TextButton downButton { "<" }, upButton { ">" };
    juce::Label rangeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoPicker)
};

} // namespace chimedes::ui
