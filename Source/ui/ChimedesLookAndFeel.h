#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace chimedes::ui
{

// Tema visual de Chimedes: controles planos, esquinas redondeadas y grises neutros.
class ChimedesLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChimedesLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawTickBox (juce::Graphics&, juce::Component&, float x, float y, float w, float h,
                      bool ticked, bool isEnabled, bool isMouseOverButton, bool isButtonDown) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
};

} // namespace chimedes::ui
