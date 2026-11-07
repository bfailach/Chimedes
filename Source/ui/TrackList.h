#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "model/Track.h"
#include "ui/IconButton.h"

namespace chimedes::ui
{

// Left panel: the project's tracks. They all play at once; the active one is
// drawn in full and edited on the canvas and in the right panel.
//   click .............. make the track active
//   double-click ....... rename
//   speaker ............ mute / unmute
//   right-click ........ rename, duplicate, delete
//   + .................. new track
class TrackList : public juce::Component
{
public:
    static constexpr int maxTracks = 16;   // one per audio engine slot

    TrackList();
    ~TrackList() override;

    std::function<void (std::size_t)> onSelect;
    std::function<void (std::size_t)> onToggleMute;
    std::function<void (std::size_t)> onDuplicate;
    std::function<void (std::size_t)> onDelete;
    std::function<void (std::size_t, juce::String)> onRename;
    std::function<void()> onAdd;

    void refresh (const model::Project& project, std::size_t activeIndex);

    // Height it needs to show every track.
    int getIdealHeight() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class Row;
    class MuteButton;

    static constexpr int headerHeight = 34;
    static constexpr int rowHeight = 34;
    static constexpr int padding = 10;

    void showRowMenu (std::size_t index);

    juce::Label header;
    IconButton addButton { "New track", IconButton::Icon::Add };
    std::vector<std::unique_ptr<Row>> rows;
    std::size_t active = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackList)
};

} // namespace chimedes::ui
