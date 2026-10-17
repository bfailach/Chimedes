#include "model/NoteNames.h"

#include <array>
#include <cctype>
#include <sstream>

namespace chimedes::model
{

namespace
{
    // Semitono de cada letra respecto a C.
    int letterSemitone (char letter)
    {
        switch (std::toupper (static_cast<unsigned char> (letter)))
        {
            case 'C': return 0;
            case 'D': return 2;
            case 'E': return 4;
            case 'F': return 5;
            case 'G': return 7;
            case 'A': return 9;
            case 'B': return 11;
            default:  return -1;
        }
    }

    bool isMidiPitch (int value) { return value >= 0 && value <= 127; }
}

std::optional<int> parsePitch (const std::string& text)
{
    if (text.empty())
        return std::nullopt;

    // Número MIDI directo.
    if (std::isdigit (static_cast<unsigned char> (text[0])))
    {
        std::size_t used = 0;
        try
        {
            const auto value = std::stoi (text, &used);
            if (used == text.size() && isMidiPitch (value))
                return value;
        }
        catch (...) {}

        return std::nullopt;
    }

    const auto semitone = letterSemitone (text[0]);
    if (semitone < 0)
        return std::nullopt;

    std::size_t i = 1;
    int accidental = 0;

    while (i < text.size() && (text[i] == '#' || text[i] == 'b'))
        accidental += text[i++] == '#' ? 1 : -1;

    int octave = 4;

    if (i < text.size())
    {
        std::size_t used = 0;
        try
        {
            octave = std::stoi (text.substr (i), &used);
        }
        catch (...)
        {
            return std::nullopt;
        }

        if (i + used != text.size())
            return std::nullopt;
    }

    const auto pitch = (octave + 1) * 12 + semitone + accidental;
    return isMidiPitch (pitch) ? std::optional<int> (pitch) : std::nullopt;
}

std::optional<std::vector<int>> parsePitches (const std::string& text)
{
    std::string normalised = text;
    for (auto& c : normalised)
        if (c == ',')
            c = ' ';

    std::istringstream stream (normalised);
    std::vector<int> pitches;
    std::string token;

    while (stream >> token)
    {
        const auto pitch = parsePitch (token);
        if (! pitch.has_value())
            return std::nullopt;

        pitches.push_back (*pitch);
    }

    if (pitches.empty())
        return std::nullopt;

    return pitches;
}

std::string pitchName (int pitch)
{
    static constexpr std::array<const char*, 12> names {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    const auto octave = pitch / 12 - 1;
    return std::string (names[static_cast<std::size_t> (pitch % 12)]) + std::to_string (octave);
}

std::string pitchNames (const std::vector<int>& pitches)
{
    std::string out;
    for (const auto pitch : pitches)
    {
        if (! out.empty())
            out += ' ';
        out += pitchName (pitch);
    }
    return out;
}

} // namespace chimedes::model
