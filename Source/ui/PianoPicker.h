#pragma once

#include <functional>
#include <set>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

namespace chimedes::ui
{

// Two-octave piano keyboard to choose a node's notes.
//
// Playing a key makes it sound and leaves it as a proposal; Enter saves it to the
// node and Esc discards it. So you can try before deciding.
//   click .................... proposes that single note
//   Shift + click ............ adds or removes the note from the proposed chord
//   computer keys ............ A W S E D F T G Y H U J K O L P, from C to the D# of
//                              the next octave, starting at the first visible C.
//                              Several keys at once make a chord; Shift + letter
//                              adds or removes the note, as with the mouse.
//   Z / X, < > ............... octave down / up
//   Enter / Esc .............. save / discard the proposal
class PianoPicker : public juce::Component
{
public:
    PianoPicker();

    // Called on save (Enter) with the notes, sorted from low to high.
    std::function<void (std::vector<int>)> onCommit;

    // A key starts (true) or stops (false) sounding.
    std::function<void (int pitch, bool isDown)> onAudition;

    // The proposal changed (to show its names and the Enter / Esc hint).
    std::function<void()> onPendingChanged;

    // The node's saved notes. If they change, the proposal is discarded; if they are
    // the same (the track was refreshed by another edit), the proposal is kept.
    // If the lowest one is out of view, the keyboard moves to it.
    void setPitches (const std::vector<int>& pitches);

    // The current proposal (equal to the saved notes if there is none).
    const std::vector<int>& getPendingPitches() const noexcept { return pending; }
    bool hasPendingChange() const noexcept { return pending != saved; }

    void commitPending();
    void discardPending();

    // Computer keyboard. The window passes keys on while the panel shows a node;
    // they return true if the key was theirs.
    bool handleKeyPress (const juce::KeyPress& key);
    bool handleKeyStateChange();

    // Turns off the notes that are sounding (on focus loss, on node change...).
    void releaseAllKeys();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    static constexpr int whiteKeysShown = 14;  // two octaves
    static constexpr int headerHeight = 24;

    int lowestPitch() const noexcept { return (baseOctave + 1) * 12; }
    int pitchAt (juce::Point<float> position) const;
    juce::Rectangle<float> keyBounds (int pitch) const;
    juce::Rectangle<float> keyboardArea() const;
    void setBaseOctave (int octave);
    void updateRangeLabel();

    // A new key is played: it sounds and changes the proposal.
    //   asChord: added to the chord (other keys held);  toggle: Shift.
    void playKey (int pitch, bool asChord, bool toggle);
    void setPending (std::vector<int> next);

    static bool isBlack (int pitch) noexcept;

    std::vector<int> saved;     // the node's notes
    std::vector<int> pending;   // the proposal (what is shown and saved with Enter)
    int baseOctave = 3;         // the keyboard starts at C3

    int mousePitch = -1;            // key held with the mouse
    std::set<int> heldKeyCodes;     // letters held down
    std::set<int> soundingPitches;  // preview notes sounding, to turn them off

    juce::TextButton downButton { "<" }, upButton { ">" };
    juce::Label rangeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoPicker)
};

} // namespace chimedes::ui
