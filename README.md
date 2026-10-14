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
undo/redo, periodic smoothing, sine/triangle/saw/square/flat starting shapes, and
dark/light themes and musical audio preview. Cycles contain 1024 or 2048 samples.
Export and project persistence are upcoming stages.

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

### Smoothing

Click Smooth... above the canvas, then adjust Smoothness. The faint curve shows
the original while the colored curve previews the result. Each slider adjustment
uses the original snapshot, so moving the slider back restores that same preview.
Zero percent restores the source exactly.

Apply and Cancel appear beside Undo/Redo while the panel is open.
Apply (or Enter) commits one undoable change. Cancel, Escape, or Undo restores the
source. Drawing, shape replacement, Clear and cycle length are paused until you
apply or cancel. Smoothing wraps around the cycle boundary and preserves length;
it can reduce peaks and does not automatically normalize the result.

To smooth one section while keeping sharp boundaries, click Smooth... first,
then check Select region. A vertical cursor guide and sample
number show where a selection will start. Drag horizontally in either direction
to create a range, then drag either vertical boundary to adjust it. Clear selection
lets you choose a new range. The selected endpoints and all outside samples stay
exactly unchanged; straight slopes are preserved. Uncheck Select region to preview
whole-cycle smoothing from the same original snapshot.

Range adjustments update the preview immediately without accumulating smoothing.
Apply is disabled until at least three samples are selected. Escape or Undo during
a range drag restores the prior range; a subsequent Escape cancels the preview.
Cancel restores the waveform and selection from before Smooth was opened.

Check Preserve edges to keep sharp corners while smoothing the curves between them.
Its Edge angle slider appears on the same row as the checkbox and starts at 30°. Points whose change
in direction exceeds the threshold stay exactly unchanged; smoothing does not
average across them. Lower angles preserve more corners; higher angles allow more
rounding. This works on either the whole cycle or a selected range.

The angle compares incoming and outgoing slopes estimated from nearby samples,
using the complete cycle width and amplitude range as normalized coordinates.
It measures a turn rather than the steepness of a straight line, and stays
independent of zoom and window size. Very small features may be treated as
unevenness by the slope estimate; lowering the threshold preserves more detail.
Edge preservation is optional and initially off.

The selection indices appear beneath Select region, with a small Clear button.
When the checkbox is off, that area stays blank. Smoothness sits to its right.

Reduce jitter is an independent checkbox for cleaning nearly straight lines and
shallow curves, initially off. Use it alone or combine it with Preserve edges.
Standalone cleanup tries a straight-line or gentle quadratic fit within the chosen
scope, keeping its original boundary samples exact. With Preserve edges also on,
stable corner peaks partition the curve into separate fitting intervals. At 100% Smoothness, accepted spans use the
clean fit; lower values blend it with the original. More complex spans keep
Gaussian smoothing. No additional dependency is required.

Reduce jitter uses a conservative starting tolerance: RMS fit error at most 0.02
amplitude and no point more than 0.04 away. Standalone cleanup uses a fixed 30°
gentle-curve limit; the Edge angle slider applies when Preserve edges is enabled.
Small intentional wiggles within that tolerance may be cleaned up too; compare
the preview before Apply. Cancel and Undo retain their exact restoration behavior.

### Audio preview

Press Play below the canvas, or Space while the canvas has focus. The note menu
covers C1–C8 and starts at A4 (440 Hz). Volume starts at 20%; it controls listening
level independently of the waveform. Play becomes Stop while sounding.

Drawing, presets, undo/redo, length changes and smoothing previews update playback
live. Start/stop and volume changes fade over 20 ms; note/table changes crossfade
over 10 ms. Audio settings selects the output device and exposes rate/buffer options
under Advanced. Device restarts stop playback; press Play again afterward.
No audio input or MIDI input is opened. Startup and the ordinary smoke checks are silent.

Preview uses harmonic-limited copies selected for the note and device sample rate.
The monitor copy removes DC and scales reconstruction overshoot without boosting
quiet waveforms. Authored/project/export samples stay unchanged. Playback defaults
to the system output when Play or Audio settings is first used; settings are
currently session-only.

### Horizontal zoom

Use the menu above the right side of the grid for Whole cycle, 2x–64x zoom, or
Sample view. Zooming from the full cycle centers on a selected range when present.
The horizontal scrollbar below the grid navigates the cycle. Sample view shows
individual sample points and grid lines, with at least 12 pixels per sample for
precise selection and drawing. The cursor guide identifies the nearest sample;
indices start at zero. Zoom changes the view only, leaving samples and history intact.
Choose Whole cycle to see the complete waveform again.

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

The smoke command disables physical audio, runs without opening a visible window, and renders both themes to
build/gui-smoke/, exercises drawing/undo/resize handlers at both sample lengths and
the minimum window size, verifies whole-cycle/regional smoothing, adjustable selection gestures, zoomed sample drawing, preview controls and offline audio/transport, and exits. The export checker supports PCM16/float32 WAV and raw;
raw files have no sample-rate metadata, so omit --rate for them.

Use a separate review build folder to try changes while keeping an existing drawing
session open:

~~~sh
mise run run -- --build-dir build/review
mise run smoke -- --build-dir build/review
~~~

Review builds reuse already-fetched dependency sources when available and keep
compiled output/CMake caches separate. Choose separate folders for app/core modes.

To verify a real output device, run:

~~~sh
mise run audio-check -- --build-dir build/audio-review
~~~

This plays a brief quiet A4 tone, checks output callbacks, and writes
build/audio-check/device-check.json. It verifies driver callback operation, not
a microphone/loopback recording or listening quality. Native macOS checks remain
outstanding; the Windows review build is build/app-audio-preview-windows.
