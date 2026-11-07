#pragma once

#include <functional>

#include "model/Track.h"

namespace chimedes::ui
{

// How UI components ask for changes to the track. Whoever owns the track
// (MainComponent) applies them, records them for undo and recomputes the schedule.
//
//   - edit: a one-off change, one undo entry.
//   - beginGesture / updateGesture / endGesture: a continuous change (dragging a
//     node, moving a slider) applied live that leaves a single entry.
struct EditCallbacks
{
    using Change = std::function<void (model::Track&)>;

    std::function<void (const Change&)> edit;
    std::function<void()> beginGesture;
    std::function<void (const Change&)> updateGesture;
    std::function<void()> endGesture;
};

} // namespace chimedes::ui
