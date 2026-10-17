#include "MainComponent.h"

#include "model/Units.h"

namespace chimedes::ui
{

MainComponent::MainComponent()
{
    setSize (1200, 760);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));

    // Regla de tiempo: una marca por pulso (100 px con el zoom al 100 %).
    constexpr int rulerHeight = 28;
    g.setColour (juce::Colour (0xff1e2128));
    g.fillRect (0, 0, getWidth(), rulerHeight);

    const auto beatWidth = static_cast<float> (model::pixelsPerBeat);
    g.setFont (12.0f);

    for (int beat = 0; static_cast<float> (beat) * beatWidth < static_cast<float> (getWidth()); ++beat)
    {
        const auto x = static_cast<float> (beat) * beatWidth;
        g.setColour (juce::Colour (0xff2a2e37));
        g.drawVerticalLine (static_cast<int> (x), static_cast<float> (rulerHeight), static_cast<float> (getHeight()));
        g.setColour (juce::Colour (0xff8a909c));
        g.drawText (juce::String (beat), static_cast<int> (x) + 4, 0, 40, rulerHeight, juce::Justification::centredLeft);
    }

    g.setColour (juce::Colour (0xff8a909c));
    g.setFont (16.0f);
    g.drawText ("Chimedes", getLocalBounds().withTrimmedTop (rulerHeight), juce::Justification::centred);
}

void MainComponent::resized()
{
}

} // namespace chimedes::ui
