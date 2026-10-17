#pragma once

#include <optional>
#include <string>
#include <vector>

// Conversión entre notas MIDI y nombres legibles, con el do central (60) como C4.
namespace chimedes::model
{

// "C4" · "F#3" · "Bb2" · "60". Sin octava se asume la 4: "E" = E4.
std::optional<int> parsePitch (const std::string& text);

// Varias notas separadas por espacios o comas: "C4 E4 G4". Devuelve nullopt si el
// texto está vacío o alguna nota no se entiende.
std::optional<std::vector<int>> parsePitches (const std::string& text);

// 61 → "C#4"
std::string pitchName (int pitch);

// { 60, 64, 67 } → "C4 E4 G4"
std::string pitchNames (const std::vector<int>& pitches);

} // namespace chimedes::model
