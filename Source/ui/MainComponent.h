#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace chimedes::ui
{

// Ventana principal. Por ahora solo un lienzo vacío con la regla de tiempo;
// el editor de grafos, los paneles y el transporte llegan en los siguientes pasos.
class MainComponent : public juce::Component
{
public:
    MainComponent();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace chimedes::ui
