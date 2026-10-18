#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace chimedes::ui
{

// Pide al sistema una barra de título oscura para la ventana (que ya debe estar visible).
// En Windows 11 usa el gris exacto del tema; en Windows 10, el modo oscuro del sistema.
// En otros sistemas no hace nada.
void applyDarkTitleBar (juce::Component& window);

} // namespace chimedes::ui
