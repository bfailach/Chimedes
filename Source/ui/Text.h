#pragma once

#include <juce_core/juce_core.h>

namespace chimedes::ui
{

// Los textos de la interfaz están en español y llevan acentos. JUCE interpreta un
// literal `const char*` como ASCII, así que todo texto visible pasa por aquí.
inline juce::String text (const char* utf8)
{
    return juce::String::fromUTF8 (utf8);
}

} // namespace chimedes::ui
