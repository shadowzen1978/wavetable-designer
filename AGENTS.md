# AGENTS.md

## Product and architecture

Wavetable Designer is a native standalone C++20/JUCE app for authoring one periodic
waveform. Windows x64 and macOS 14+ Universal 2 are product targets. Electron is
explicitly excluded. Read README.md and docs/implementation-plan.md before editing.
The selected language is C++20/JUCE; docs/language-options.md records alternatives.
SZ404 in ts404-remake is the priority export consumer. Browser mockups and a later
browser version are expected; retain a portable core and a versioned data contract.

The current phase is editor implementation. Freehand drawing, stroke undo/redo,
Clear/presets and periodic length changes are implemented. Smoothing, persistence,
export and audio preview are upcoming stages.
The primary UI is the drawing box; dark is default and light is required.
Place drawing and persistence controls above the canvas, and transport/export
controls below it. Cycle length belongs with the bottom export controls.

## Layout and dependencies

- src/core is JUCE-free C++: Waveform holds sample data; EditorModel owns gestures,
  preset identity, and bounded undo/redo snapshots. Length changes resample the
  current curve; they must not regenerate a starting shape or discard edits.
- src/app is the JUCE GUI/lifecycle; future audio and file adapters belong under
  src/audio and src/io when implemented. Do not add empty placeholder subsystems.
- tests is the headless doctest suite; core-only configure must not fetch JUCE.
- tools/dev.py is the cross-platform build driver. Windows MSVC setup uses vswhere
  and tools/msvc-env.cmd. CMake presets keep app/core/platform caches separate.
- JUCE 8.0.4 and doctest 2.4.11 are pinned to exact revisions in CMakeLists.txt.
- mise pins Python, uv, CMake and Ninja. Python/uv are development tooling only;
  pyproject.toml/uv.lock provide NumPy and soundfile for independent export checks.
- Keep generated build trees, virtual environments and scratch files ignored.

## Commands

From the repository root, first run mise trust, mise install, and mise run setup.
If a shell has not activated mise, these tasks still provide the selected tools.

- mise run build: native app plus core tests.
- mise run build-core: JUCE-free core and test binaries.
- mise run test: build the core and run CTest.
- mise run run: build and launch the native app.
- mise run smoke: build and run hidden GUI initialization/theme snapshot checks
  plus synthetic canvas gestures and undo/redo/clear/resize/shortcut verification.
  Snapshots are written under build/gui-smoke/ for visual inspection.
- mise run check-exports -- <file> --samples 1024 --encoding pcm16 --rate 44100:
  independent mono WAV validation. Omit --rate for raw files.

Native Mac builds require Xcode command line tools. macOS presets are configured
but not yet run on a Mac; do not claim Windows validation verifies macOS behavior.

## Working practices and contracts

- Inspect git status first and preserve unrelated work. Do not commit or push unless
  instructed. The initial scaffold/layout was committed as 2bd9da8.
- Keep accepted decisions and milestone evidence under docs/. Update commands as
  implementation changes; do not treat historical language comparisons as gates.
- Sister repos are tooling and compatibility references, not required dependencies.
  Do not copy their plugin installation, DAW or reverse-engineering harnesses.
- Add meaningful numerical, file-format and audio checks; avoid tests that merely
  confirm placeholders or mirror GUI construction.
- A cycle has exactly N finite amplitudes at phase i/N. Do not duplicate phase 1.
- Drawing fills skipped indices and stores one amplitude per sample. Periodic
  smoothing previews from a stable source snapshot and commits one undo action.
- Do not force first/last equality or silently normalize/repair seams.
- Preserve floating-point authored/project data; quantize an export copy only.
- SZ404 raw is PCM16 LE, exactly 1024 samples for one cycle. Its WAV importer
  truncates at 1024; explicitly resample a copy for its profile when needed.
- WAV sample rate, cycle length, device sample rate and note pitch are distinct.
  Keep preview gain/harmonic limiting independent from exported authored samples.
- Keep allocation, file I/O, blocking locks and memory reclamation out of audio
  callbacks. Verify ownership and click-free table publication when preview lands.
