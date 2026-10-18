#include "WindowStyle.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <dwmapi.h>
 #pragma comment (lib, "dwmapi.lib")

 #include "ui/Theme.h"
#endif

namespace chimedes::ui
{

void applyDarkTitleBar ([[maybe_unused]] juce::Component& window)
{
   #if JUCE_WINDOWS
    auto* peer = window.getPeer();
    if (peer == nullptr)
        return;

    auto* hwnd = static_cast<HWND> (peer->getNativeHandle());

    // Constantes de DWM por número: los SDK antiguos no tienen sus nombres.
    constexpr DWORD useImmersiveDarkMode = 20;  // Windows 10 (2004) y posteriores
    constexpr DWORD captionColour = 35;         // Windows 11
    constexpr DWORD textColour = 36;            // Windows 11

    const BOOL dark = TRUE;
    DwmSetWindowAttribute (hwnd, useImmersiveDarkMode, &dark, sizeof (dark));

    const auto toColorRef = [] (juce::Colour c) { return RGB (c.getRed(), c.getGreen(), c.getBlue()); };
    const COLORREF caption = toColorRef (theme::background);
    const COLORREF text = toColorRef (theme::textPrimary);

    // En Windows 10 estas dos fallan sin más; queda el modo oscuro del sistema.
    DwmSetWindowAttribute (hwnd, captionColour, &caption, sizeof (caption));
    DwmSetWindowAttribute (hwnd, textColour, &text, sizeof (text));
   #endif
}

} // namespace chimedes::ui
