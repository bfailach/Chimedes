#pragma once

#include <optional>
#include <string>
#include <vector>

// Conversion between MIDI notes and readable names, with middle C (60) as C4.
namespace chimedes::model
{

// "C4" · "F#3" · "Bb2" · "60". Without an octave, 4 is assumed: "E" = E4.
std::optional<int> parsePitch (const std::string& text);

// Several notes separated by spaces or commas: "C4 E4 G4". Returns nullopt if the
// text is empty or any note cannot be understood.
std::optional<std::vector<int>> parsePitches (const std::string& text);

// 61 → "C#4"
std::string pitchName (int pitch);

// { 60, 64, 67 } → "C4 E4 G4"
std::string pitchNames (const std::vector<int>& pitches);

} // namespace chimedes::model
