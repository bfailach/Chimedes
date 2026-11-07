#pragma once

#include <juce_core/juce_core.h>

namespace chimedes::ui
{

// JUCE reads a `const char*` literal as ASCII, so any visible text with non-ASCII
// characters (accents, symbols such as · or …) goes through here.
inline juce::String text (const char* utf8)
{
    return juce::String::fromUTF8 (utf8);
}

} // namespace chimedes::ui
