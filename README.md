# Wavetable Designer

A simple UI interface to create single-cycle waveform for audio synthesis and export them to wav or raw formats.

Export sizes should include 1024 and 2048 samples, with mono PCM16 and float32 WAV
at 44.1 and 48 kHz. A dedicated SZ404 preset should export one complete 1024-sample
cycle as PCM16 WAV or little-endian PCM16 raw.

The editor should allow for hand drawing waveforms in a visual editor window, with restrictions to ensure a single point per sample.
A smoothing function should be included that can smooth out curves and straight lines, with a slider determining how much smoothness should be applied

Bezier curve option would be nice but not strictly necessary for a first pass MVP.

Audio preview should be present to hear the cycle in a musical context as a simple oscillator with basic pitch shifting to different notes.

The implementation uses C++20 and JUCE. This is a standalone app only; no plugin
formats are needed. The waveform core stays independent of JUCE to support future
ports and a possible browser version.

The UX of the product should be simple and intuitive with the focus on the drawing box and basic editing tools.  More advanced settings can be hidden behind a setting button.
Light mode and dark mode themes are a must with the dark mode theme as a default.

## Development

The implementation is a native C++20/JUCE GUI application with freehand drawing,
undo/redo, sine/triangle/saw/square/flat starting shapes, and dark/light themes.
Cycles contain 1024 or 2048 samples. Smoothing, export, project persistence and audio
preview are upcoming stages.

### Drawing

Click and drag inside the waveform grid to draw. Dragging fills every sample
between pointer events; dragging backward redraws that region. Drags outside the
grid clamp to the edges. Release the button to finish one undoable stroke, or press
Escape while drawing to cancel it.

Undo uses Ctrl+Z on Windows or Cmd+Z on macOS. Redo uses Ctrl/Cmd+Shift+Z (also
Ctrl+Y on Windows). Clear resets the cycle to silence. Selecting a starting shape
replaces the curve. Clear and shape replacement are both undoable.

Changing cycle length periodically resamples the current curve and can be undone.
The dropdown displays Custom after hand drawing. History retains up to 128 actions;
project save/open is still to come.

The planned SZ404 export preset uses a complete 1024-sample mono PCM16 cycle;
generic exports also support float32 WAV. See [the implementation plan](docs/implementation-plan.md)
and [compatibility findings](docs/language-options.md).

Install mise and a native C++ compiler: Visual Studio Build Tools with the C++
workload on Windows, or Xcode command line tools on macOS. From the repo root:

~~~sh
mise trust
mise install
mise run setup
mise run build
mise run test
mise run run
~~~

mise pins CMake/Ninja/Python/uv. CMake fetches pinned JUCE and doctest revisions.
Python dependencies are development tools and are not part of the shipped app.
The first build needs network access for the native dependencies.

Windows uses x64 MSVC. macOS presets target macOS 14+ Universal 2; native Mac
validation remains outstanding. App/core/platform build caches are separate:

~~~sh
mise run build-core
mise run smoke
mise run check-exports -- path/to/cycle.wav --samples 1024 --encoding pcm16 --rate 44100
~~~

The smoke command runs without opening a visible window, renders both themes to
build/gui-smoke/, exercises drawing/undo/resize handlers at both sample lengths and
the minimum window size, and exits. The export checker supports PCM16/float32 WAV and raw;
raw files have no sample-rate metadata, so omit --rate for them.
