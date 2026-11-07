#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace chimedes::ui
{

// Asks the system for a dark title bar for the window (which must already be visible).
// On Windows 11 it uses the theme's exact grey; on Windows 10, the system dark mode.
// Does nothing on other systems.
void applyDarkTitleBar (juce::Component& window);

} // namespace chimedes::ui
