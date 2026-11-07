#include "TrackGlance.h"

#include "ui/Theme.h"

namespace chimedes::ui
{

TrackGlance::TrackGlance()
{
    graphView.setBottomCornerRadius (theme::cornerRadius);
    addAndMakeVisible (graphView);

    title.setFont (theme::sans (14.0f));
    title.setColour (juce::Label::textColourId, theme::textPrimary);
    title.setColour (juce::Label::textWhenEditingColourId, theme::textPrimary);
    title.setColour (juce::Label::backgroundWhenEditingColourId, theme::control);
    title.setColour (juce::Label::outlineWhenEditingColourId, theme::focus);
    title.setInterceptsMouseClicks (false, false);
    title.onTextChange = [this]
    {
        if (onRename)
            onRename (title.getText().trim());
    };
    addAndMakeVisible (title);

    detail.setFont (theme::sans (12.0f));
    detail.setColour (juce::Label::textColourId, theme::textSecondary);
    detail.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (detail);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    expandButton.onClick = [this] { if (onExpand) onExpand(); };
    discardButton.onClick = [this] { if (onDiscard) onDiscard(); };

    for (auto* b : { &closeButton, &expandButton, &discardButton })
        addAndMakeVisible (b);
}

void TrackGlance::setTrackInfo (const juce::String& name, const juce::String& instrument)
{
    if (! title.isBeingEdited())
        title.setText (name, juce::dontSendNotification);
    detail.setText (instrument, juce::dontSendNotification);
}

juce::Rectangle<int> TrackGlance::cardBounds() const
{
    return getLocalBounds().withTrimmedRight (buttonColumnWidth);
}

void TrackGlance::paint (juce::Graphics& g)
{
    const auto card = cardBounds().toFloat();

    // Header strip: panel colour with the top corners rounded.
    juce::Path header;
    header.addRoundedRectangle (card.getX(), card.getY(), card.getWidth(), static_cast<float> (headerHeight),
                                theme::cornerRadius, theme::cornerRadius, true, true, false, false);
    g.setColour (theme::panel);
    g.fillPath (header);

    // Round background behind the side buttons, like Zen's.
    for (auto* b : { &closeButton, &expandButton, &discardButton })
    {
        g.setColour (theme::panel);
        g.fillEllipse (b->getBounds().toFloat());
        g.setColour (theme::panelOutline);
        g.drawEllipse (b->getBounds().toFloat().reduced (0.5f), 1.0f);
    }
}

void TrackGlance::paintOverChildren (juce::Graphics& g)
{
    // Outline of the whole card, over the canvas, and the line under the header.
    const auto card = cardBounds().toFloat();
    g.setColour (theme::panelOutline);
    g.drawRoundedRectangle (card.reduced (0.5f), theme::cornerRadius, 1.0f);
    g.drawHorizontalLine (headerHeight, card.getX(), card.getRight());
}

void TrackGlance::resized()
{
    auto card = cardBounds();
    auto header = card.removeFromTop (headerHeight).reduced (16, 0);
    graphView.setBounds (card.withTrimmedTop (1));

    detail.setBounds (header.removeFromRight (180));
    detail.setJustificationType (juce::Justification::centredRight);
    title.setBounds (header);

    // Buttons in a column to the right of the card, starting at its top.
    auto column = getLocalBounds().removeFromRight (buttonColumnWidth).withTrimmedLeft (10);
    for (auto* b : { &closeButton, &expandButton, &discardButton })
    {
        b->setBounds (column.removeFromTop (36).withSizeKeepingCentre (36, 36));
        column.removeFromTop (8);
    }
}

void TrackGlance::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Double-click on the header renames the track.
    if (e.y < headerHeight && cardBounds().contains (e.getPosition()))
        title.showEditor();
}

} // namespace chimedes::ui
