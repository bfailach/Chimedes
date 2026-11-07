#pragma once

#include <string>

#include "model/Track.h"

namespace chimedes::model
{

// Chimedes file format (.chimedes): readable JSON, with options written as
// words ("timeline", "bfs"...) instead of numbers.
//
// {
//   "format": "chimedes", "version": 1, "bpm": 120,
//   "tracks": [ { "name": "", "directed": true, "mode": "timeline", ...,
//                 "start": 1, "end": 5,
//                 "nodes": [ { "id": 1, "x": 0, "y": 0, "pitches": [60], ... } ],
//                 "edges": [ { "id": 1, "from": 1, "to": 2 } ] } ]
// }
//
// When reading, missing fields take their default value, out-of-range values are
// clamped and edges to nodes that do not exist are dropped: a hand-edited file
// must never leave the app in an impossible state.

inline constexpr const char* projectFileExtension = ".chimedes";
inline constexpr int currentProjectFormatVersion = 1;

std::string projectToJson (const Project& project);

struct ProjectLoadResult
{
    bool ok = false;
    std::string error;   // ready to show to the user as is
    Project project;
};

ProjectLoadResult projectFromJson (const std::string& json);

} // namespace chimedes::model
