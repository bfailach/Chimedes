#include "ChimedesLookAndFeel.h"

#include "ui/Theme.h"

namespace chimedes::ui
{

ChimedesLookAndFeel::ChimedesLookAndFeel()
    : juce::LookAndFeel_V4 ({ theme::background,      // windowBackground
                              theme::control,         // widgetBackground
                              theme::panel,           // menuBackground
                              theme::outline,         // outline
                              theme::textPrimary,     // defaultText
                              theme::controlHover,    // defaultFill
                              theme::textPrimary,     // highlightedText
                              theme::controlPressed,  // highlightedFill
                              theme::textPrimary })   // menuText
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::background);

    setColour (juce::Label::textColourId, theme::textPrimary);

    setColour (juce::TextButton::buttonColourId, theme::control);
    setColour (juce::TextButton::buttonOnColourId, theme::controlPressed);
    setColour (juce::TextButton::textColourOffId, theme::textPrimary);
    setColour (juce::TextButton::textColourOnId, theme::textPrimary);

    setColour (juce::ComboBox::backgroundColourId, theme::control);
    setColour (juce::ComboBox::textColourId, theme::textPrimary);
    setColour (juce::ComboBox::arrowColourId, theme::textSecondary);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::focusedOutlineColourId, theme::focus);

    setColour (juce::PopupMenu::backgroundColourId, theme::panel);
    setColour (juce::PopupMenu::textColourId, theme::textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::controlHover);
    setColour (juce::PopupMenu::highlightedTextColourId, theme::textPrimary);

    setColour (juce::Slider::textBoxTextColourId, theme::textPrimary);
    setColour (juce::Slider::textBoxBackgroundColourId, theme::control);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, theme::controlPressed);

    setColour (juce::TextEditor::backgroundColourId, theme::control);
    setColour (juce::TextEditor::textColourId, theme::textPrimary);
    setColour (juce::TextEditor::highlightColourId, theme::controlPressed);
    setColour (juce::TextEditor::highlightedTextColourId, theme::textPrimary);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, theme::focus);
    setColour (juce::CaretComponent::caretColourId, theme::textPrimary);

    setColour (juce::ToggleButton::textColourId, theme::textPrimary);
    setColour (juce::ToggleButton::tickColourId, theme::textPrimary);
    setColour (juce::ToggleButton::tickDisabledColourId, theme::textDisabled);
}

void ChimedesLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                bool isMouseOverButton, bool isButtonDown)
{
    auto colour = theme::control;
    if (isButtonDown || button.getToggleState()) colour = theme::controlPressed;
    else if (isMouseOverButton)                  colour = theme::controlHover;

    if (! button.isEnabled())
        colour = colour.withMultipliedAlpha (0.5f);

    g.setColour (colour);
    g.fillRoundedRectangle (button.getLocalBounds().toFloat(), theme::controlRadius);
}

void ChimedesLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));

    auto colour = box.isMouseOver (true) ? theme::controlHover : theme::control;
    if (! box.isEnabled())
        colour = colour.withMultipliedAlpha (0.5f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, theme::controlRadius);

    if (box.hasKeyboardFocus (true))
    {
        g.setColour (theme::focus);
        g.drawRoundedRectangle (bounds.reduced (0.5f), theme::controlRadius, 1.0f);
    }

    // Chevron hacia abajo.
    const auto cx = static_cast<float> (width) - 16.0f;
    const auto cy = static_cast<float> (height) * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);

    g.setColour (box.isEnabled() ? theme::textSecondary : theme::textDisabled);
    g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ChimedesLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void ChimedesLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float, float,
                                            juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    const auto cy = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const auto left = static_cast<float> (x) + 6.0f;
    const auto right = static_cast<float> (x + width) - 6.0f;
    const auto thumbX = juce::jlimit (left, right, sliderPos);

    // Pista completa y tramo recorrido.
    g.setColour (theme::controlPressed);
    g.fillRoundedRectangle (left, cy - 2.0f, right - left, 4.0f, 2.0f);
    g.setColour (theme::textSecondary);
    g.fillRoundedRectangle (left, cy - 2.0f, thumbX - left, 4.0f, 2.0f);

    g.setColour (slider.isEnabled() ? theme::textPrimary : theme::textDisabled);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ thumbX, cy }));
}

void ChimedesLookAndFeel::drawTickBox (juce::Graphics& g, juce::Component& component, float x, float y,
                                       float w, float h, bool ticked, bool isEnabled, bool, bool)
{
    const auto box = juce::Rectangle<float> (x, y, w, h).reduced (2.0f);

    g.setColour (ticked ? theme::textSecondary : theme::control);
    g.fillRoundedRectangle (box, 3.0f);

    if (ticked)
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
        tick.lineTo (box.getX() + box.getWidth() * 0.42f, box.getBottom() - box.getHeight() * 0.25f);
        tick.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.25f);

        g.setColour (isEnabled ? theme::panel : theme::textDisabled);
        g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::ignoreUnused (component);
}

void ChimedesLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (theme::panel);
    g.setColour (theme::outline);
    g.drawRect (0, 0, width, height);
}

void ChimedesLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height), theme::controlRadius);
}

void ChimedesLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    const auto colour = editor.hasKeyboardFocus (true) && ! editor.isReadOnly()
                            ? editor.findColour (juce::TextEditor::focusedOutlineColourId)
                            : editor.findColour (juce::TextEditor::outlineColourId);

    if (colour.isTransparent())
        return;

    g.setColour (colour);
    g.drawRoundedRectangle (0.5f, 0.5f, static_cast<float> (width) - 1.0f, static_cast<float> (height) - 1.0f,
                            theme::controlRadius, 1.0f);
}

} // namespace chimedes::ui
