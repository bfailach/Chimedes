#include "ui/Theme.h"

#include "BinaryData.h"

namespace chimedes::ui::theme
{

// Fonts are loaded once from the data bundled in the executable
// (Resources/Fonts, SIL OFL license), so they look the same on every system.
juce::Typeface::Ptr sansTypeface()
{
    static const auto typeface = juce::Typeface::createSystemTypefaceFor (BinaryData::Sora_ttf,
                                                                          BinaryData::Sora_ttfSize);
    return typeface;
}

juce::Typeface::Ptr monoTypeface()
{
    static const auto typeface = juce::Typeface::createSystemTypefaceFor (BinaryData::DMMono_ttf,
                                                                          BinaryData::DMMono_ttfSize);
    return typeface;
}

} // namespace chimedes::ui::theme
