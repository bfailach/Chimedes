# Chimedes

A desktop app for making generative music by drawing graphs. Nodes are notes or chords; edges are the connections between them. Music comes out of walking the graph, either along a traditional timeline or with search algorithms.

Written in C++20 with [JUCE](https://juce.com) 9.

## Status

Phase 1 (minimum version) done:

- [x] Project skeleton: CMake, JUCE and a window with the time ruler
- [x] Model and engine: connectivity validation, trees (BFS, DFS, Dijkstra, Prim), traversals and schedules in both modes, with tests
- [x] Audio: transport with BPM, a basic synthesizer and thread-safe schedule publishing
- [x] Editable canvas, properties panel, undo and redo
- [x] Optional beat snapping in algorithm mode
- [x] Dark UI with floating panels, a keyboard note picker, six built-in instruments and a Random button

Phase 2 done:

- [x] Algorithm mode: trees, traversals, siblings in sequence or together, and beat snapping
- [x] Save and open projects (`.chimedes`)

Phase 3 done:

- [x] Several tracks playing at once, each with its own cycle
- [x] Live editing while playing
- [x] Your own sound samples as instruments

Phase 4 done:

- [x] Randomize the graph, the notes, the rhythm, the instruments or the algorithm, separately or all together
- [x] Export to WAV
- [x] Export to MIDI

## Usage

The app opens with a demo graph; the **New** button (sheet icon) starts an empty one. The canvas fills the whole window, with floating panels: tracks on the left, file actions at the top, properties on the right and the transport at the bottom.

| Action | How |
| --- | --- |
| Create a node | Double-click on empty space (it inherits the notes of the last selected node) |
| Move a node | Drag from its centre |
| Connect | Drag from the edge of a node to another |
| Select | Click a node or an edge |
| Delete | Delete or Backspace, with something selected |
| Start and end | Right-click a node, or the buttons in the right panel |
| Node notes | Keyboard in the right panel: a click proposes a note and plays it, Shift+click builds chords; `<` `>` change octave. The proposal gets a white outline and is saved with Enter |
| Instrument | Right panel: one for the track and, if you like, another for each node |
| Try notes | Click the panel's keyboard or press **A W S E D F T G Y H U J K O L P** (from C to the D♯ of the next octave, starting at the first visible C). They sound right away. Several letters at once, or Shift, make a chord; **Z / X** change octave |
| Save the note or chord | **Enter** (Esc discards the proposal) |
| New, open, save | Sheet, folder and floppy-disk icons in the top bar (next to the project name), or **Ctrl+N**, **Ctrl+O**, **Ctrl+S** (**Ctrl+Shift+S**: save as). Before closing, opening another project or starting a new one, the app asks if there are unsaved changes |
| Tracks | Left panel: a click makes a track active (the one you edit; the others are dimmed and keep playing), a double-click renames it, the speaker mutes it, **+** adds one (up to 16) and a right-click duplicates or deletes. Clicking a dimmed star also activates its track. Everything can be undone |
| Random | **Random** button (dice icon): a menu to randomize the graph, notes, rhythm, instruments or algorithm of the active track, everything at once, or everything on every track. Can be undone |
| Export | Tray icon in the top bar: **WAV** (audio, 48 kHz, 24-bit) or **MIDI**, choosing the length |
| Zoom | Mouse wheel |
| Pan | Drag on empty space, or with the middle button |
| Play / pause | Space bar |
| Undo / redo | Arrow icons in the bottom bar, Ctrl+Z / Ctrl+Y or Ctrl+Shift+Z |

The first two nodes of an empty track become the start and the end automatically. Node rings: green = start, violet = end, orange = unreachable. Note names show faintly next to each node and brighten on hover or selection; the time ruler only appears in timeline mode. Nodes that cannot be reached from the start are marked in orange, and the track stays silent until that is fixed.

### Look: a star atlas

The canvas is drawn as a star map on soft black (the *Starlight* theme). Each node is a star whose size follows its velocity. During playback:

- a spark travels along the edge towards the next note;
- the star flashes in its note's colour and sends out ripples in neighbouring colours (in a chord, the last ripple takes its other note's colour);
- the path played stays as a pastel trail that fades when the next cycle starts.

Colours follow the colour wheel by pitch class: C red, D orange, E green, G blue, A violet, B magenta. A C is the same colour in any octave, and the selected keys on the panel's keyboard use the same colours. The rest of the UI is almost monochrome, with starlight white as the only accent.

Fonts: [Space Grotesk](https://github.com/floriankarsten/space-grotesk) for the UI and [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) for figures, both under the SIL Open Font License (see `Resources/Fonts`). They are bundled in the executable.

## Several tracks

Each track is an independent graph with its own options (mode, tree, traversal, instrument) and its own cycle: a 3-beat track against a 4-beat one gives a polyrhythm that repeats every 12 beats. All tracks share the BPM and the canvas; the active one is drawn in full and the others dimmed, still flashing as they play. New tracks get a different instrument so they can be told apart by ear.

## Projects

A project is saved as `.chimedes`: a readable JSON file with the BPM, the track options and every node and edge on its own line. The file name appears in the top bar and in the window title, with a dot (•) when there are unsaved changes. On Windows a project can be opened by dragging it onto `Chimedes.exe` or passing it as an argument.

When opening, missing fields take their default value, out-of-range values are clamped and edges to nodes that do not exist are dropped, so a hand-edited file never leaves the app in an impossible state. If the file is damaged, the message says on which line. When saving, a temporary file is written first: if anything fails, the previous file stays intact.

## Instruments

Six synthesized instruments, with no audio files: organ, electric piano (FM), bell (inharmonic FM), plucked string (Karplus-Strong), pad (two detuned, filtered saws) and bass. Each instrument plays on its own MIDI channel, so several can live in the same track.

### Your own samples

In the instrument drop-down (of the track or of a node), **Load sample…** opens a WAV, AIFF, FLAC or OGG file (up to 30 seconds) and uses it as an instrument: the sample is taken as recorded at middle C (C4) and transposed for each note. A project holds up to ten samples; when there is no room, the ones nobody uses are removed. Loading the same file again reuses the sample.

The project stores the file's path, not the audio. If the sample is inside the project folder, the path is saved as relative, so the whole folder can be moved or shared. If a file is missing when opening, a warning appears and that sample stays silent; the rest of the project plays normally.

## Random

The dice opens a menu. Everything applies to the active track, except the last option, and can be undone:

- **New graph**: see below.
- **Notes**: same shape, new scale and key; the melody follows the edges from the start and the end goes back to the tonic.
- **Rhythm**: new durations and velocities, with one character for the whole track (shorter or longer, more even or more accented). The end node keeps its duration, so the cycle stays the same length.
- **Instruments**: the track's and, sometimes, a few nodes with another one, from the built-ins and the loaded samples.
- **Algorithm** (in algorithm mode): tree (Prim only on undirected graphs), traversal, siblings and beat snapping.
- **Everything**, and **Everything, on every track** (each with its own roll, in a single undo step).

The new graph is random: 5 to 14 nodes; a chain, branches, fan or web shape; nodes spread evenly or bunched; variable vertical spread; and an overall length of 4 to 16 beats. Notes land on a grid of beats or half beats and the last one ends exactly when the cycle does, so the cycle lasts a whole number of beats and the music follows the BPM. Notes come from a random scale and key, with the odd chord, and the last note goes back to the tonic. The graph can always be played and the view adjusts to show all of it.

## Export

From the tray icon in the top bar. Before choosing the file you choose the length, with how long it lasts at the current BPM:

- **Full loop**: until every track lines up again (the least common multiple of their cycles; a 3-beat track and a 4-beat one give 12). If that goes over 4096 beats, as many loops of the longest track as fit.
- **1, 2, 4 or 8 loops** of the longest track.

Only unmuted tracks play.

- **WAV**: the audio exactly as it sounds, with the same engine, instruments and samples, rendered faster than real time with a progress bar and a cancel button. Stereo, 48 kHz, 24-bit, with a 2-second tail at the end so notes can die out. If the mix goes over 0 dB, the whole thing is turned down so it does not clip.
- **MIDI**: a standard file (format 1) with a tempo track and one MIDI track per track, with its name. Each instrument goes on its own channel (channel 10, General MIDI percussion, is never used) and the built-ins get a similar General MIDI program: organ, electric piano, tubular bells, nylon guitar, warm pad and bass. Samples get no program: assign whatever sound you like in the destination app.

## Building

Clone with submodules, because JUCE lives in `JUCE/`:

```sh
git clone --recurse-submodules https://github.com/bfailach/Chimedes.git
cd Chimedes
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

If you already cloned without submodules: `git submodule update --init`.

On Linux, JUCE needs these headers (Ubuntu/Debian):

```sh
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxext-dev libxi-dev libxrender-dev libxcomposite-dev \
  libfreetype-dev libfontconfig1-dev libgl1-mesa-dev
```

On Windows, Visual Studio 2022 or later is enough; on macOS, Xcode.

## Tests

The model and the engine do not depend on JUCE, so their tests build in seconds:

```sh
cmake -S . -B build-tests -DCHIMEDES_BUILD_APP=OFF
cmake --build build-tests
./build-tests/chimedes_tests
```

## Rendering without a sound card

`ChimedesRender` is built along with the app and renders to WAV with the same audio engine: the demo graph, or a project using the same export as the app (`ChimedesRender output.wav project.chimedes [loops, 0 = full]`):

```sh
./build/ChimedesRender_artefacts/Debug/ChimedesRender demo.wav timeline 2 3
```

Modes: `timeline`, `seq-bfs`, `seq-dfs`, `sim-bfs`. Then come the number of cycles and, optionally, the instrument (1 organ, 2 electric piano, 3 bell, 4 plucked string, 5 pad, 6 bass). The tool prints the expected time of each note next to the onsets it detects in the audio.

## Structure

```text
Source/
├── model/    Project, tracks, nodes and edges (plain C++)
├── engine/   Validation, trees, traversals, schedules, sequencer and export (plain C++)
├── audio/    Audio engine, synthesizers and offline rendering (JUCE)
├── ui/       JUCE user interface
└── Main.cpp  Application entry point
Tools/        Development tools (ChimedesRender)
Tests/        Engine tests
JUCE/         Git submodule (JUCE 9.0.4)
```

## How a graph sounds

The engine turns each track into a **schedule**: the list of notes in one cycle with their starting beat. Audio only plays that schedule.

- **Timeline mode:** each node sounds at beat `x / 100` (100 px = 1 beat). The cycle runs from the start node until the end node finishes sounding.
- **Algorithm mode:** the graph is turned into a tree rooted at the start node (BFS, DFS, Dijkstra or Prim) and walked with BFS or DFS. The wait between nodes is the length of the edge joining them. The siblings of a branch can sound in sequence or together.
