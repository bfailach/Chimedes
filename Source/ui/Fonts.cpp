#include "ui/Theme.h"

#include "BinaryData.h"

namespace chimedes::ui::theme
{

// Las fuentes se cargan una vez desde los datos incluidos en el ejecutable
// (Resources/Fonts, licencia SIL OFL), así se ven igual en todos los sistemas.
juce::Typeface::Ptr sansTypeface()
{
    static const auto typeface = juce::Typeface::createSystemTypefaceFor (BinaryData::SpaceGrotesk_ttf,
                                                                          BinaryData::SpaceGrotesk_ttfSize);
    return typeface;
}

juce::Typeface::Ptr monoTypeface()
{
    static const auto typeface = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMono_ttf,
                                                                          BinaryData::JetBrainsMono_ttfSize);
    return typeface;
}

} // namespace chimedes::ui::theme
