#pragma once

namespace chimedes::model
{

// At 100 % zoom, 100 canvas pixels are 1 beat.
// At 120 BPM, 1 second is 200 pixels.
inline constexpr double pixelsPerBeat = 100.0;

inline constexpr double pixelsToBeats (double pixels) noexcept { return pixels / pixelsPerBeat; }
inline constexpr double beatsToPixels (double beats) noexcept  { return beats * pixelsPerBeat; }

} // namespace chimedes::model
