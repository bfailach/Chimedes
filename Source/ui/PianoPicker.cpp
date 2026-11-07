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

    // For each semitone of the octave: for white keys, the key index (C = 0 … B = 6);
    // for black keys, the boundary between the two white neighbours, where they are centred.
    constexpr float semitoneSlot[12] = { 0.0f, 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 4.0f, 4.0f, 5.0f, 5.0f, 6.0f, 6.0f };

    constexpr int minOctave = -1;  // C-1 = MIDI note 0
    constexpr int maxOctave = 8;   // the view ends at B9, beyond MIDI 127

    // Computer keyboard letters, in semitones from the first visible C:
    // the middle row are the white keys and the row above, the black ones.
    constexpr const char* keyLayout = "awsedftgyhujkolp";

    int semitoneForKey (int keyCode)
    {
        const auto lower = juce::CharacterFunctions::toLowerCase (static_cast<juce::juce_wchar> (keyCode));
        for (int i = 0; keyLayout[i] != 0; ++i)
            if (keyLayout[i] == lower)
                return i;
        return -1;
    }

    bool hasCommandModifier (const juce::ModifierKeys& mods)
    {
        return mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown();
    }
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
    auto sorted = newPitches;
    std::sort (sorted.begin(), sorted.end());

    if (sorted == saved)
        return;   // the same selection: the proposal in progress is kept

    saved = sorted;
    pending = sorted;

    if (onPendingChanged)
        onPendingChanged();

    if (! saved.empty())
    {
        const auto lowest = saved.front();
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
    // If any chosen note is out of view, the range is painted as a warning.
    const auto hidden = std::any_of (pending.begin(), pending.end(), [this] (int p)
    {
        return p < lowestPitch() || p >= lowestPitch() + 24;
    });

    rangeLabel.setText (juce::String (model::pitchName (lowestPitch())) + juce::String::fromUTF8 (" – ")
                            + juce::String (model::pitchName (lowestPitch() + 23)),
                        juce::dontSendNotification);
    rangeLabel.setColour (juce::Label::textColourId, hidden ? theme::accent : theme::textSecondary);
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
    // Black keys are on top: check them first.
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
    // Letters reach the window: give it focus back in case another control had it.
    for (auto* p = getParentComponent(); p != nullptr; p = p->getParentComponent())
        if (p->getWantsKeyboardFocus())
        {
            p->grabKeyboardFocus();
            break;
        }

    const auto pitch = pitchAt (e.position);
    if (pitch < 0)
        return;

    mousePitch = pitch;
    playKey (pitch, false, e.mods.isShiftDown());
}

void PianoPicker::mouseUp (const juce::MouseEvent&)
{
    if (mousePitch >= 0 && soundingPitches.erase (mousePitch) > 0 && onAudition)
        onAudition (mousePitch, false);

    mousePitch = -1;
    repaint();
}

void PianoPicker::playKey (int pitch, bool asChord, bool toggle)
{
    if (pitch < 0 || pitch > 127)
        return;

    auto next = pending;
    const auto it = std::find (next.begin(), next.end(), pitch);

    if (toggle)
    {
        if (it == next.end())
            next.push_back (pitch);
        else if (next.size() > 1)  // a node always has at least one note
            next.erase (it);
    }
    else if (asChord)
    {
        if (it == next.end())
            next.push_back (pitch);
    }
    else
    {
        next = { pitch };
    }

    // It sounds even if the key was already in the proposal: playing is listening.
    const bool removed = toggle && it != pending.end() && std::find (next.begin(), next.end(), pitch) == next.end();
    if (! removed && onAudition)
    {
        if (soundingPitches.count (pitch) > 0)
            onAudition (pitch, false);
        soundingPitches.insert (pitch);
        onAudition (pitch, true);
        repaint();
    }

    setPending (std::move (next));
}

void PianoPicker::setPending (std::vector<int> next)
{
    std::sort (next.begin(), next.end());
    if (next == pending)
        return;

    pending = std::move (next);
    updateRangeLabel();
    repaint();

    if (onPendingChanged)
        onPendingChanged();
}

void PianoPicker::commitPending()
{
    if (! hasPendingChange())
        return;

    saved = pending;
    if (onCommit)
        onCommit (pending);
    if (onPendingChanged)
        onPendingChanged();
    repaint();
}

void PianoPicker::discardPending()
{
    setPending (saved);
}

void PianoPicker::releaseAllKeys()
{
    if (onAudition)
        for (const auto p : soundingPitches)
            onAudition (p, false);

    if (! soundingPitches.empty())
        repaint();

    soundingPitches.clear();
    heldKeyCodes.clear();
    mousePitch = -1;
}

bool PianoPicker::handleKeyPress (const juce::KeyPress& key)
{
    if (hasCommandModifier (key.getModifiers()))
        return false;

    if (key == juce::KeyPress::returnKey)
    {
        if (! hasPendingChange())
            return false;
        commitPending();
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (! hasPendingChange())
            return false;
        discardPending();
        return true;
    }

    const auto code = juce::CharacterFunctions::toLowerCase (static_cast<juce::juce_wchar> (key.getKeyCode()));
    if (code == 'z' || code == 'x')
    {
        releaseAllKeys();
        setBaseOctave (baseOctave + (code == 'z' ? -1 : 1));
        return true;
    }

    // Note letters are handled in handleKeyStateChange (on press and release);
    // here they are only claimed so they do nothing else.
    return semitoneForKey (key.getKeyCode()) >= 0;
}

bool PianoPicker::handleKeyStateChange()
{
    const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
    bool handled = false;

    for (int i = 0; keyLayout[i] != 0; ++i)
    {
        // As in JUCE's MIDI keyboard: the lowercase letter; each system maps it
        // to its physical key.
        const auto lower = static_cast<int> (keyLayout[i]);
        const bool down = juce::KeyPress::isKeyCurrentlyDown (lower);
        const bool wasDown = heldKeyCodes.count (lower) > 0;
        const auto pitch = lowestPitch() + i;

        if (down && ! wasDown && ! hasCommandModifier (mods))
        {
            // Another letter is already held: this one joins the chord.
            const bool asChord = ! heldKeyCodes.empty();
            heldKeyCodes.insert (lower);
            playKey (pitch, asChord, mods.isShiftDown());
            handled = true;
        }
        else if (! down && wasDown)
        {
            heldKeyCodes.erase (lower);
            if (soundingPitches.erase (pitch) > 0 && onAudition)
                onAudition (pitch, false);
            repaint();
            handled = true;
        }
    }

    return handled;
}

void PianoPicker::paint (juce::Graphics& g)
{
    const auto isSelected = [this] (int p) { return std::find (pending.begin(), pending.end(), p) != pending.end(); };
    const auto isSaved = [this] (int p) { return std::find (saved.begin(), saved.end(), p) != saved.end(); };
    const bool proposing = hasPendingChange();
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
                colour = theme::pitchColour (p);   // the key takes its note's colour
            if (! playable)
                colour = theme::control;

            // Key sounding: a little lighter.
            if (soundingPitches.count (p) > 0)
                colour = colour.interpolatedWith (juce::Colours::white, 0.35f);

            g.setColour (colour);
            g.fillRoundedRectangle (key, 2.0f);

            // Unsaved proposal: new keys get a starlight-white outline;
            // saved keys about to be removed get a dot in their colour.
            if (proposing && isSelected (p) && ! isSaved (p))
            {
                g.setColour (theme::accent);
                g.drawRoundedRectangle (key.reduced (1.0f), 2.0f, 1.5f);
            }
            else if (proposing && isSaved (p) && ! isSelected (p))
            {
                g.setColour (theme::pitchColour (p));
                g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ key.getCentreX(), key.getBottom() - 8.0f }));
            }

            // A label on every C, to find your way.
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
