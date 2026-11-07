# Changelog

All notable changes to Chimedes are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Changed

- The app now opens with *Carousel*, an original waltz for kalimba drawn as a ring of stars that rise and fall with the melody.

## [0.1.0] - 2026-11-07

First public release, with a ready-to-run Windows build.

### Download

- **`Chimedes-0.1.0-windows-x64.exe`**: the app on its own. Download it and run it; nothing to install.
- **`Chimedes-0.1.0-windows-x64.zip`**: the same executable with the licence files.
- **`SHA256SUMS.txt`**: checksums to verify the downloads.

Windows may show a SmartScreen warning because the executable is not code-signed yet: choose **More info → Run anyway**.

### Added

- **Graph editor**: create nodes with a double-click, connect them by dragging, set the start and end nodes, undo and redo everything, zoom and pan.
- **Two ways to play a graph**: *timeline* mode (a node's horizontal position is its beat) and *algorithm* mode (the graph becomes a BFS, DFS, Dijkstra or Prim tree that is walked with BFS or DFS, with siblings in sequence or together and optional beat snapping).
- **Notes and chords** from an on-screen keyboard or the computer keyboard, with live preview.
- **22 synthesized instruments** in six families (keys, mallets, strings, winds, synth and bass), plus your own WAV, AIFF, FLAC or OGG samples.
- **Up to 16 tracks** playing at once, each with its own cycle, so tracks of different lengths make polyrhythms; mute, rename, duplicate and a floating window for new tracks.
- **Randomize** the graph, notes, rhythm, instruments or algorithm of a track, or everything on every track, in one undoable step.
- **Projects** saved as readable `.chimedes` JSON files, with safe saving and tolerant loading.
- **Export** to WAV (stereo, 48 kHz, 24-bit) and to standard MIDI files.
- **Starlight theme**: the canvas is a star atlas that lights up in each note's colour during playback.

[Unreleased]: https://github.com/bfailach/Chimedes/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/bfailach/Chimedes/releases/tag/v0.1.0
