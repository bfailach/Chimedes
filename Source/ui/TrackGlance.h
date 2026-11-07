#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/GraphView.h"
#include "ui/IconButton.h"

namespace chimedes::ui
{

// A floating window inside the main window that shows a single track, in the
// style of Zen Browser's "Glance": a rounded card over the dimmed canvas, with its
// own canvas to edit the track and a column of buttons beside it.
//   close (Esc, or a click outside) ... keep the track and go back
//   expand ............................. open it on the main canvas as the active track
//   discard ............................ delete the track (can be undone)
// The card's header shows the track name (double-click to rename) and its instrument.
class TrackGlance : public juce::Component
{
public:
    static constexpr int buttonColumnWidth = 48;   // to the right of the card
    static constexpr int headerHeight = 44;

    TrackGlance();

    GraphView& graph() noexcept { return graphView; }

    std::function<void()> onClose;
    std::function<void()> onExpand;
    std::function<void()> onDiscard;
    std::function<void (juce::String)> onRename;

    void setTrackInfo (const juce::String& name, const juce::String& instrument);

    // The card, in this component's coordinates (the button column sits to its right).
    juce::Rectangle<int> cardBounds() const;

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    GraphView graphView;
    juce::Label title, detail;
    IconButton closeButton { "Close (Esc)", IconButton::Icon::Close };
    IconButton expandButton { "Open on the main canvas", IconButton::Icon::Expand };
    IconButton discardButton { "Discard this track", IconButton::Icon::Delete };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackGlance)
};

// Translucent layer over the main canvas while a track window is open. A click on it
// closes the window, as in Zen Browser.
class DimOverlay : public juce::Component
{
public:
    DimOverlay() { setInterceptsMouseClicks (true, false); }

    std::function<void()> onClick;

    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black.withAlpha (0.55f)); }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (onClick)
            onClick();
    }
};

} // namespace chimedes::ui
