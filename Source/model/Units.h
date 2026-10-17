#pragma once

namespace chimedes::model
{

// Con el zoom al 100 %, 100 píxeles del lienzo equivalen a 1 pulso.
// A 120 BPM, 1 segundo son 200 píxeles.
inline constexpr double pixelsPerBeat = 100.0;

inline constexpr double pixelsToBeats (double pixels) noexcept { return pixels / pixelsPerBeat; }
inline constexpr double beatsToPixels (double beats) noexcept  { return beats * pixelsPerBeat; }

} // namespace chimedes::model
