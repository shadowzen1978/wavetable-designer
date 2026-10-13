# Wavetable Designer: implementation plan

Status: C++20 with native JUCE selected; scaffold and freehand drawing implemented on
feat/wavetable-designer-scaffold. Windows/macOS, lengths, preview and persistence
are agreed. Export profiles below are the implementation defaults for compatibility.
Date: 2026-10-13.
Source of product intent: [README](../README.md).

## Product intent and MVP

A local, standalone Windows and macOS desktop tool for creating one periodic waveform, auditioning it
at musical pitches, and exporting exactly one cycle. The drawing area is the main
focus. Advanced controls belong in settings. Dark is the default theme; light is
also required. No plugin formats or DAW integration are required.

UI layout: reserve the space above the drawing canvas for drawing and persistence
controls. Transport and export options belong below it, with cycle length in the
bottom export area. Omit the "Shape a single cycle" subtitle.

Required by the README:
- Hand drawing with exactly one amplitude per sample.
- Adjustable smoothing for drawn curves and straight segments.
- Mono, single-cycle WAV and raw exports at 1024/2048 samples, with target-specific encoding (see export profiles).
- WAV sample-rate choices of 44.1 and 48 kHz.
- Oscillator preview at different notes.
- Simple editing tools, settings, and light/dark themes.

Agreed preview and persistence scope, with basic editing defaults:
- Undo/redo, reset, and sine/triangle/saw/square starting shapes.
- Save/open a versioned project so edits are not lost between sessions.
- Play/stop, note selection, and independent preview volume.

Optional later addition: DC removal and normalization as explicit user actions.

Deferred: Bezier tools, multi-frame wavetables, synthesis modulation, effects,
external MIDI, arbitrary audio import, spectral editing, and installer polish.

## Accepted decisions and remaining choices

| Decision | Current state |
| --- | --- |
| Language/frontend | Selected: C++20 with a native JUCE GUI application |
| Platforms | Windows and macOS are both product targets; Linux is not in current scope |
| Lengths | 1024 and 2048 samples; user corrected README |
| Compatibility priority | Build a waveform library for SZ404 in ts404-remake |
| Encoding | SZ404 profile: PCM16 WAV/raw. Generic profile: float32 or PCM16 WAV |
| Preview | Simple monophonic note preview, volume, envelopes, harmonic limiting |
| Persistence | Undo/redo and versioned project save/open accepted |
| Browser future | Browser release after desktop iteration; browser UI mockups expected earlier |
| Excluded | Electron is explicitly rejected; do not propose or install it |

## Language and frontend options

See [language-options.md](language-options.md) for Python/Qt, C++/JUCE, C++/Qt,
browser reuse, the LyricPrompter2 reference, and export evidence.

Python makes numerical experiments and tests convenient, but native Qt widgets do
not automatically transfer to HTML/JavaScript. Both Python/PySide6 and C++/Qt can
embed web content using Qt WebEngine and WebChannel; that is a separate architecture
choice and introduces Chromium. Do not assume the user's rejection of Electron is
permission to choose a different embedded-browser product architecture.

Selected approach: native C++20/JUCE desktop app plus small Python numerical
validation tools and future browser mockups. Keep core math free of JUCE to allow
a future JavaScript port or an Emscripten/Wasm build. Python/Qt and C++/Qt are
recorded as researched alternatives; they are not current implementation targets.

Estimates are rough focused engineering days for one experienced developer. They
include the agreed editor, smoothing, preview, exports, save/open and themes. They
exclude browser product implementation, signing/notarization, public installers,
and Bezier tools. Allow 30-50% contingency; access to a Mac is needed for native QA.

| Architecture | Desktop MVP on primary dev machine | Additional macOS build/package/device validation |
| --- | --- | --- |
| C++20 + native JUCE | 8-15 days | 3-6 days |
| Python + native PySide6 + NumPy | 8-16 days | 3-7 days |
| C++20 + native Qt6 | 10-18 days | 3-7 days |
| Python/Qt shell + HTML/JavaScript UI | 10-18 days | 4-8 days |

The hybrid route trades extra integration and packaging work now for greater UI
reuse later. Estimates overlap substantially and should not decide the stack alone.

## Waveform and interaction contract

Represent a cycle as N finite amplitudes in [-1, 1], at phases i/N for
0 <= i < N. The phase-1 endpoint is not duplicated in the exported array.

Drawing maps horizontal position to a sample index and vertical position to an
amplitude. Interpolate across indices skipped between pointer events; clamp at the
canvas edges. Repeated positions overwrite their sample rather than adding points.
Backward strokes are valid. One stroke is one undo action. At 2048 samples, many
samples share screen pixels, so interpolated drawing is essential; magnified sample
editing can be deferred.

Smoothing operates periodically: neighbors at the right edge wrap to the left edge.
Proposed initial method is a circular low-pass convolution with a bounded slider
mapping. Recompute slider previews from the pre-adjustment snapshot rather than
repeatedly smoothing an already-smoothed result. Apply commits one undo action;
Cancel restores the snapshot. Preserve length and finite amplitudes. Smoothing
reduces high harmonics and may lower peaks; do not silently normalize afterward.

Do not force the first and last samples to be equal: valid saw waves contain a
wrap discontinuity, and samples lie at different phases. Optional seam repair/DC
removal must be explicit and separate from the export writer.

Choose the editing length explicitly. If length changes, periodically resample the
cycle as an undoable edit. Export should report its exact length and encoding;
never append a duplicated endpoint or export a multi-second preview recording.

## Preview and export are separate

Preview pitch uses phase increment = note frequency / audio device sample rate.
The selected WAV header sample rate does not control preview pitch. An N-sample
file played directly at Fs repeats at Fs/N; a consuming wavetable synth may instead
interpret it as one cycle at its own oscillator pitch.

Preview should have short start/stop envelopes and conservative independent gain.
Sharp drawn shapes contain high harmonics: interpolation alone does not make their
playback band-limited. Proposed implementation builds harmonic-limited table levels
away from the audio callback, then selects/crossfades them for the current note.
Preview filtering must not replace the authored samples used for export.

Publish completed preview tables through a bounded, preallocated handoff. Keep
allocation, file I/O, blocking locks, and final memory destruction out of the audio
callback. Crossfade updates to avoid clicks during editing. Verify the ownership
and handoff design before implementing it.

WAV export is mono with exactly N frames and the selected profile encoding. 44.1/48 kHz
is header metadata for this cycle; selecting it does not resample or change N.
Raw export has no header, sample-rate metadata, or self-described encoding. Define
sample type and endianness in the UI/docs; PCM16 payload is N * 2 bytes and float32
payload is N * 4 bytes. Raw float32 is not compatible with SZ404's raw loader.
Export neither silently normalizes nor smooths the waveform.

## Export profiles

- SZ404: one 1024-sample mono cycle; PCM16 WAV at 44.1 kHz by default, with
  PCM16 little-endian raw as an alternative (2048 bytes, no prefix/header).
- Generic software: mono float32 WAV, 1024 or 2048 samples, 44.1/48 kHz.
  2048/44.1 kHz is a useful software-synth convention, not a universal device format.
- PCM16 WAV remains available for compatibility. Add additional named device
  profiles only after checking their documented import requirements.

Keep editing/project data floating point; quantize only the export copy. A named
SZ404 export from a 2048-sample project must periodically resample a copy to 1024
and clearly show the conversion. It must never export the first half of the cycle
or silently change the source project. Generic export preserves selected N.

Local code inspection: SZ404's loadWavetableFile reads only the first 1024 WAV
frames, mixes channels to mono, ignores sample-rate metadata, and quantizes with
lround(clamp(v, -1, 1) * 32767). Its raw importer overlays the first 1024 little-endian
signed 16-bit samples. Its JUCE WAV reader supports PCM32 and float32, but the
stored oscillator table is still int16. A 2048-frame cycle is truncated, not resampled.
See language-options.md for exact local source locations. These are inspected
contracts; no exported file has been runtime-tested through SZ404 yet.

## Sister-repository tooling findings

Inspected locally:
- ts404-remake: .mise.toml, pyproject.toml, plugin/CMakeLists.txt,
  plugin/CMakePresets.json, plugin/scripts/build.cmd and test.cmd.
- delay-plugin-remakes: .mise.toml, pyproject.toml, SZ3D/CMakeLists.txt,
  SZ3D/CMakePresets.json and SZ3D/scripts/build-core.cmd.

Both use Python 3.12, uv, CMake, and Ninja through mise, with separate .venv,
.venv-macos, and .venv-wsl directories. Both use C++20, a JUCE-free DSP library,
CTest/doctest, and pinned FetchContent dependencies. JUCE is pinned to 8.0.4;
doctest to 2.4.11. These are local reference versions, not claims about the newest
release. The scaffold pins these reference versions to exact revisions.

Reuse conventions, not entire configs: this app needs no plugin installation,
REAPER smoke test, reverse-engineering dependencies, or original-product assets.
Verify preset schema/minimums and commands before copying reference config.

## Selected tooling and support targets

- JUCE 8.0.4: exact revision 51d11a2be6d5c97ccf12b4e5e827006e19f0555a.
- doctest 2.4.11: exact revision ae7a13539fb71f270b87eb2e874fbac80bc8dda2.
- mise pins Python 3.12.13, uv 0.11.16, CMake 4.4.0 and Ninja 1.13.2.
- Python is tooling only: NumPy and soundfile, resolved by uv.lock. No Python
  dependency is linked or required by the shipped native application.
- Windows x64 uses MSVC, discovered through vswhere; no hard-coded VS version.
- macOS presets target 14+ Universal 2 (arm64/x86_64), matching the sister repos.
  These are configured support targets and need native Mac validation.
- Separate app/core/platform build directories avoid incompatible CMake caches.
- UV_PROJECT_ENVIRONMENT selects .venv, .venv-macos, or .venv-linux. A .python-version
  aligns uv with mise's pinned Python. No automatic environment creation by mise.
- tools/dev.py drives build/test/run on both product platforms; only its Windows
  environment helper uses cmd. No global install or plugin copying on build.

The build tools were found through the host's mise installation. Explicit mise
execution resolves the session's unactivated PATH. Native dependencies are fetched
into ignored build trees; clean users do not require sister repositories.
The current branch is feat/wavetable-designer-scaffold. Initial scaffold/layout
commit: 2bd9da8. Subsequent drawing work is a separate uncommitted change.

## C++ layout

~~~text
CMakeLists.txt / CMakePresets.json
.mise.toml
AGENTS.md
docs/implementation-plan.md
src/core/       waveform, drawing interpolation, smoothing, resampling
src/audio/      oscillator and table publication
src/io/         WAV/raw export and versioned project state
src/app/        JUCE lifecycle, components, commands, themes
tests/          core, format, and offline audio behavior
tools/          portable build driver, MSVC helper and independent export checks
build*/         ignored, separate caches for app/core/platform configurations
~~~

Targets: wavetable_core, wavetable_tests, and WavetableDesigner. An app-off/core-only
configure must avoid fetching/building JUCE. Use juce_add_gui_app for the product.
Use a pinned doctest dependency for meaningful numerical/format tests. Add JSON
only when implementing project persistence, rather than copying every sibling dep.

Windows build wrappers locate MSVC using vswhere, initialize x64, then invoke
CMake/Ninja. No system installation on build. Separate full-app and core-only
presets prevent toggling one CMake cache between incompatible configurations.

## Implementation sequence and completion criteria

1. Completed: select native C++20/JUCE and establish export profiles.
2. Completed: create feat/wavetable-designer-scaffold and commit the initial
   scaffold/layout at the user's request (2bd9da8). Preserve unrelated user work.
3. Completed on Windows: set up selected tools/dependencies, pinned versions and
   ignores; establish
   build/test/run tasks and finalize AGENTS.md with actual commands. Scaffold a
   launchable dark/light window, waveform view with starting shapes, and core target.
   Verified: native configure/build, core contract tests, hidden app startup, and
   visual inspection of dark/light snapshots. macOS native validation is outstanding.
4. Drawing/model/undo/presets implemented; periodic smoothing remains next.
   Completion: skipped/reversed strokes and boundary smoothing are verified;
   slider adjustments do not accumulate and undo restores exact prior samples.
5. Implement export and project save/open.
   Completion: independent reader verifies WAV format/rate/frame count, raw byte
   count/endianness, and sample agreement; project round-trip restores authored data.
   Verify SZ404 imports a whole cycle; test profile resampling, quantization and
   actual WAV/raw import paths with golden cases.
6. Implement preview and device settings.
   Completion: offline checks verify note frequency, wrapping, update transitions,
   and harmonic limiting; manual device/play-stop/editing checks show stable output.
7. Refine layout, keyboard commands, themes and errors. Smoke-test all selected
   platforms. Record platform results separately; buildability is not release QA.

For the recommended route, a rough breakdown within the 8-15 day MVP range is
1-2 days scaffold, 3-5 editor/smoothing, 1-2 export/state, 2-4 preview, and about
1-2 UX/verification, with overlap between tasks. Re-estimate after the drawing and
preview prototypes. Public release packaging and additional-platform QA are a
separate estimate once hardware/support targets are known.

## Scaffold validation (2026-10-13)

- Branch: feat/wavetable-designer-scaffold; scaffold/layout later committed as 2bd9da8.
- Windows x64 MSVC configure/build completed using mise's pinned tools.
- Core-only CTest passed; its dependency tree contains doctest only, with no JUCE.
- Native JUCE app initialized and exited cleanly in smoke mode. Dark/light 960x640
  snapshots were visually inspected for layout and contrast.
- Eight valid/invalid WAV/raw fixture checks passed through the independent Python
  checker; mise's task argument forwarding and locked tooling imports were verified.
- These checks verify the scaffold/checker, not an implemented product export path
  or actual SZ404 import. Those remain in the export milestone.
- Native macOS build, application startup and device behavior are not yet tested.
- Pinned doctest emits nonfatal legacy CMake/new-MSVC compatibility warnings.

## Drawing implementation and validation (2026-10-13)

Implemented in the JUCE-free core and native canvas:
- Each left-button gesture is one undo action. Sparse and reverse strokes linearly
  interpolate all traversed indices. The latest visit to an index overwrites it.
- Mapping selects the nearest sample at phase i/N and clamps phase/amplitude at the
  canvas boundaries. Right-clicks and initial clicks in padding do not draw.
- Escape cancels the pending gesture; keyboard focus loss commits it. Undo during
  a gesture cancels that stroke without consuming older history.
- Clear/flat and preset replacement are undoable; Custom identifies drawn curves.
- Cycle-length changes use periodic linear interpolation, preserving the authored
  curve rather than rebuilding a preset. These changes are also undoable.
- Up to 128 snapshot actions are retained. New edits invalidate redo; cancelled
  and no-op gestures preserve it. Save/open remains a future stage.

Verification on Windows: the headless numerical suite passed for sparse/reverse
strokes, repeated indices, endpoint clipping, exact undo/redo, cancelled/no-op
strokes, invalid-input rejection, bounded history and periodic resampling. Hidden
GUI checks exercised actual mouse handlers, control callbacks and keyboard command
handlers at 1024/2048 samples and 640x420 minimum size; default/minimum snapshots
were visually inspected. These synthetic handler checks do not claim physical
mouse/device testing or native macOS validation.

Next: periodic smoothing with non-cumulative slider previews. Persistence/export
and audio preview follow according to the milestones.
