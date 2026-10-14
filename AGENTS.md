# AGENTS.md

## Product and architecture

Wavetable Designer is a native standalone C++20/JUCE app for authoring one periodic
waveform. Windows x64 and macOS 14+ Universal 2 are product targets. Electron is
explicitly excluded. Read README.md and docs/implementation-plan.md before editing.
The selected language is C++20/JUCE; docs/language-options.md records alternatives.
SZ404 in ts404-remake is the priority export consumer. Browser mockups and a later
browser version are expected; retain a portable core and a versioned data contract.

The current phase is editor implementation. Freehand drawing, stroke undo/redo,
Clear/presets, periodic length changes, whole-cycle/regional smoothing and audio
preview are implemented. Versioned project save/open and WAV/raw export are implemented.
The primary UI is the drawing box; dark is default and light is required.
Place drawing and persistence controls above the canvas, and transport/export
controls below it. Cycle length belongs with the bottom export controls.

## Layout and dependencies

- src/core is JUCE-free C++: PreviewBank/PreviewEngine provide FFT-limited monitor
  tables, bounded publication and the oscillator. Waveform holds sample data; EditorModel owns gestures,
  preset identity, and bounded undo/redo snapshots. Length changes resample the
  current curve; they must not regenerate a starting shape or discard edits.
- src/app is the JUCE GUI/lifecycle. src/audio/AudioPreview adapts to JUCE output
  devices and builds immutable tables on a worker. ProjectFile/Export file adapters belong
  under src/io. Export is JUCE-free; JSON/chooser/file adapters use JUCE. Do not add empty placeholder subsystems.
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
- mise run run: build and launch the native app. Use -- --build-dir build/review
  to keep an existing drawing session open while building a separate review app.
  Keep app/core review directories separate; dependency source overrides are local.
- mise run smoke: build and run hidden GUI initialization/theme snapshot checks
  plus synthetic canvas gestures, undo/redo/clear/resize/shortcuts and smoothing
  preview/apply/cancel, selection gestures, boundary isolation and offline audio.
  Physical devices are disabled in this mode. Snapshots are written under
  build/gui-smoke/ for visual inspection.
- mise run audio-check: short low-volume system-output check; report under
  build/audio-check/device-check.json. This emits audio and checks callbacks, not
  acoustic/loopback quality. Use an isolated --build-dir for review.
- mise run check-exports -- <file> --samples 1024 --encoding pcm16 --rate 44100:
  independent mono WAV validation. Omit --rate for raw files. Add --project/--profile
  to check exact authored sample agreement.
- mise run check-files: independently verify all 24 native smoke export fixtures.
- mise exec -- python tools/check_sz404_import.py: optional Windows actual-import
  check using existing sister-repo Release libraries; never builds/modifies that repo.

Native Mac builds require Xcode command line tools. macOS presets are configured
but not yet run on a Mac; do not claim Windows validation verifies macOS behavior.

## Working practices and contracts

- Inspect git status first and preserve unrelated work. Do not commit or push unless
  instructed. Scaffold/layout: 2bd9da8. User-approved drawing: c8ed303.
- Keep accepted decisions and milestone evidence under docs/. Update commands as
  implementation changes; do not treat historical language comparisons as gates.
- Sister repos are tooling and compatibility references, not required dependencies.
  Do not copy their plugin installation, DAW or reverse-engineering harnesses.
- Add meaningful numerical, file-format and audio checks; avoid tests that merely
  confirm placeholders or mirror GUI construction.
- A cycle has exactly N finite amplitudes at phase i/N. Do not duplicate phase 1.
- Drawing fills skipped indices and stores one amplitude per sample. Periodic
  smoothing previews from a stable source snapshot and commits one undo action.
- Smoothing is a preview transaction: recompute every slider adjustment from the
  captured source. Apply creates one undo action; cancel/zero strength restores
  exact original samples. Other sample edits are blocked during the preview.
- Selected-region smoothing must leave its endpoints and every outside sample
  bit-for-bit unchanged, and must not average across its hard boundaries. Smooth
  residuals from the endpoint-to-endpoint slope; straight slopes are no-ops.
  Selection changes are non-destructive, have no waveform undo action, and are
  available inside the Smooth preview context. Adjust ranges against the immutable
  source; Cancel restores the pre-preview range. Preserve/remap selection in history.
- Smooth scope is a checkbox: Select region checked means selection, unchecked
  means whole cycle. Preserve edges is optional; show its angle slider only when
  enabled (default 30 degrees), on the same row as its checkbox. Compute turns from fitted neighboring tangents in
  normalized phase/half-amplitude geometry, never screen coordinates. Fixed corner
  samples partition smoothing intervals so no averaging crosses a detected edge.
  Detect only inside a selected range; all previews use the immutable source.
- Reduce jitter is independent of Preserve edges and starts off; expose it whenever
  Smooth is open. Fit close straight/shallow quadratic spans within the chosen
  scope, with a fixed 30-degree gentle-fit limit when edge preservation is off.
  Hidden edge-angle settings must not affect standalone cleanup. When combined,
  confirm corners at two scales, retain direction-change peaks/both step sides,
  and fit between the protected anchors. Fall back to Gaussian for complex spans.
  Fit tolerance must not depend on zoom. Keep clean lines/quadratics as no-ops.
- Place range indices beneath Select region; blank that area when unchecked.
  Smoothness belongs to its right. Preserve edges and its angle share a row.
- Use the same horizontal viewport transform for drawing, selection, cursor guides,
  boundary handles and rendering. Zoom/panning never resample or create undo actions;
  Sample view exposes individual indices at a minimum of 12 pixels per sample.
- Preserve in-memory drawings when rebuilding: prefer a separate review build
  until save/open exists; do not silently close a user's unsaved editing session.
- Do not force first/last equality or silently normalize/repair seams.
- Preserve floating-point authored/project data; quantize an export copy only.
- SZ404 raw is PCM16 LE, exactly 1024 samples for one cycle. Its WAV importer
  truncates at 1024; explicitly resample a copy for its profile when needed.
- WAV sample rate, cycle length, device sample rate and note pitch are distinct.
  Keep preview gain/harmonic limiting independent from exported authored samples.
- Keep allocation, file I/O, blocking locks and memory reclamation out of audio
  callbacks. Verify ownership and click-free table publication when preview lands.

## Audio preview contract

- Playback comes before persistence/export at the user's request. Footer controls
  are Play/Stop, C1–C8 note, preview volume, output settings and cycle length.
  Space toggles transport from the canvas. Startup is silent; devices open lazily
  with zero input channels. Device restarts stop playback.
- PreviewBank builds 2048-point Fourier monitor tables with 1023 >> level maximum
  harmonics (10 levels), no DC, and a shared overshoot-only scale. Source samples
  must stay exact. Choose a level with highest partial <= 0.45 * device rate;
  mute notes above that guard. Export header rate is independent of pitch.
- PreviewEngine has four preallocated slots and a single producer/consumer mailbox.
  Producer CAS claims free slots; publish exchanges the newest ready index.
  Consumer holds current/previous banks through a 10 ms crossfade; only then marks
  the old slot free. Replaced ready banks are freed by the producer. Callback
  controls/counters use lock-free atomics, with 20 ms gain ramps and max gain 0.25.
  No callback allocation, locks, shared_ptr destruction or dynamic bank reclamation.
- Main snapshots changed samples only. The worker coalesces pending edits, builds
  off the audio thread and publishes complete banks. GUI smoke uses the same engine
  and worker with physical audio disabled. Real-device verification is separate.

## Project and file contract

- main is the GitHub default/integration branch; start new milestones on feature
  branches. Do not commit/push unless the user requests it.
- Read docs/project-format.md when changing project or export contracts.
- Projects are version-1 UTF-8 JSON .wtd files. Preserve float32 samples, preset
  identity and selection. Validate the whole file before editor replacement;
  reject future versions, invalid types/counts/amplitudes/ranges and oversized JSON.
- Open/New reset undo/redo and viewport and stop preview; Save retains history.
  Compare document data to the saved checkpoint for dirty state, including Undo.
- New/Open/Quit guard unsaved changes. Cancel or failed Save prevents document
  replacement/quit. Closing a smoothing preview first asks Apply/Discard/Cancel.
- File chooser callbacks must guard component lifetime. Freeze editing during
  file dialogs. Save As/Export confirm overwrite after finalizing the extension.
- Write to a sibling temporary file, verify write/flush, then replace the target.
  Failure must not advance the saved checkpoint or mutate authored samples.
- Do not save/export uncommitted smoothing previews. Settings remain session-only;
  project persistence stores document data, not device configuration or history.
- Export defaults to SZ404 PCM16 WAV/44.1 kHz. Offer generic PCM16/float32 WAV/raw.
  Show exported count/encoding/rate or raw LE bytes and explicit resampling.
  A float WAV contains a fact frame count. Quantize PCM with lround(v*32767).
