#include "PianoPicker.h"

#include <algorithm>

#include "model/NoteNames.h"
#include "ui/Theme.h"

namespace chimedes::ui
{

namespace
{
    const juce::Colour whiteKey      { 0xffcfcfcf };
    const juce::Colour whiteKeyHover { 0xffe0e0e0 };
    const juce::Colour blackKey      { 0xff1a1a1a };

    // Para cada semitono de la octava: en las blancas, el índice de la tecla (C = 0 … B = 6);
    // en las negras, la frontera entre las dos blancas vecinas, donde se centran.
    constexpr float semitoneSlot[12] = { 0.0f, 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 4.0f, 4.0f, 5.0f, 5.0f, 6.0f, 6.0f };

    constexpr int minOctave = -1;  // C-1 = nota MIDI 0
    constexpr int maxOctave = 8;   // la vista acaba en B9, más allá del MIDI 127
}

PianoPicker::PianoPicker()
{
    downButton.onClick = [this] { setBaseOctave (baseOctave - 1); };
    upButton.onClick = [this] { setBaseOctave (baseOctave + 1); };

    rangeLabel.setJustificationType (juce::Justification::centred);
    rangeLabel.setColour (juce::Label::textColourId, theme::textSecondary);
    rangeLabel.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (downButton);
    addAndMakeVisible (upButton);
    addAndMakeVisible (rangeLabel);
    updateRangeLabel();
}

bool PianoPicker::isBlack (int pitch) noexcept
{
    const auto s = ((pitch % 12) + 12) % 12;
    return s == 1 || s == 3 || s == 6 || s == 8 || s == 10;
}

void PianoPicker::setPitches (const std::vector<int>& newPitches)
{
    pitches = newPitches;
    std::sort (pitches.begin(), pitches.end());

    if (! pitches.empty())
    {
        const auto lowest = pitches.front();
        if (lowest < lowestPitch() || lowest >= lowestPitch() + 24)
            setBaseOctave (lowest / 12 - 1);
    }

    updateRangeLabel();
    repaint();
}

void PianoPicker::setBaseOctave (int octave)
{
    baseOctave = std::clamp (octave, minOctave, maxOctave);
    downButton.setEnabled (baseOctave > minOctave);
    upButton.setEnabled (baseOctave < maxOctave);
    updateRangeLabel();
    repaint();
}

void PianoPicker::updateRangeLabel()
{
    // Si alguna nota elegida queda fuera de vista, el rango se pinta en rojo como aviso.
    const auto hidden = std::any_of (pitches.begin(), pitches.end(), [this] (int p)
    {
        return p < lowestPitch() || p >= lowestPitch() + 24;
    });

    rangeLabel.setText (juce::String (model::pitchName (lowestPitch())) + juce::String::fromUTF8 (" – ")
                            + juce::String (model::pitchName (lowestPitch() + 23)),
                        juce::dontSendNotification);
    rangeLabel.setColour (juce::Label::textColourId, hidden ? theme::node : theme::textSecondary);
}

juce::Rectangle<float> PianoPicker::keyboardArea() const
{
    return getLocalBounds().withTrimmedTop (headerHeight + 4).toFloat();
}

juce::Rectangle<float> PianoPicker::keyBounds (int pitch) const
{
    const auto area = keyboardArea();
    const auto whiteWidth = area.getWidth() / static_cast<float> (whiteKeysShown);

    const auto offset = pitch - lowestPitch();
    const auto octave = offset / 12;
    const auto slot = static_cast<float> (octave * 7) + semitoneSlot[offset % 12];

    if (isBlack (pitch))
        return { area.getX() + (slot - 0.3f) * whiteWidth, area.getY(), whiteWidth * 0.6f, area.getHeight() * 0.6f };

    return { area.getX() + slot * whiteWidth, area.getY(), whiteWidth, area.getHeight() };
}

int PianoPicker::pitchAt (juce::Point<float> position) const
{
    // Las negras están encima: se miran primero.
    for (int pass = 0; pass < 2; ++pass)
        for (int p = lowestPitch(); p < lowestPitch() + 24; ++p)
            if (isBlack (p) == (pass == 0) && p >= 0 && p <= 127 && keyBounds (p).contains (position))
                return p;

    return -1;
}

void PianoPicker::mouseMove (const juce::MouseEvent&) { repaint(); }
void PianoPicker::mouseExit (const juce::MouseEvent&) { repaint(); }

void PianoPicker::mouseDown (const juce::MouseEvent& e)
{
    const auto pitch = pitchAt (e.position);
    if (pitch < 0 || ! onChange)
        return;

    auto next = pitches;

    if (e.mods.isShiftDown())
    {
        const auto it = std::find (next.begin(), next.end(), pitch);
        if (it == next.end())
            next.push_back (pitch);
        else if (next.size() > 1)  // un nodo siempre tiene al menos una nota
            next.erase (it);
    }
    else
    {
        next = { pitch };
    }

    std::sort (next.begin(), next.end());

    if (next != pitches)
        onChange (next);
}

void PianoPicker::paint (juce::Graphics& g)
{
    const auto isSelected = [this] (int p) { return std::find (pitches.begin(), pitches.end(), p) != pitches.end(); };
    const auto hovered = isMouseOver() ? pitchAt (getMouseXYRelative().toFloat()) : -1;

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int p = lowestPitch(); p < lowestPitch() + 24; ++p)
        {
            if (isBlack (p) != (pass == 1))
                continue;

            auto key = keyBounds (p).reduced (pass == 0 ? 0.5f : 0.0f, 0.0f);
            const bool playable = p >= 0 && p <= 127;

            auto colour = isBlack (p) ? blackKey : (p == hovered ? whiteKeyHover : whiteKey);
            if (isSelected (p))
                colour = theme::node;
            if (! playable)
                colour = theme::control;

            g.setColour (colour);
            g.fillRoundedRectangle (key, 2.0f);

            // Etiqueta en cada do, para orientarse.
            if (p % 12 == 0 && ! isSelected (p))
            {
                g.setColour (juce::Colour (0xff555555));
                g.setFont (9.0f);
                g.drawText (juce::String (model::pitchName (p)), key.removeFromBottom (14.0f), juce::Justification::centred);
            }
        }
    }
}

void PianoPicker::resized()
{
    auto header = getLocalBounds().removeFromTop (headerHeight);
    downButton.setBounds (header.removeFromLeft (28));
    upButton.setBounds (header.removeFromRight (28));
    rangeLabel.setBounds (header);
}

} // namespace chimedes::ui
