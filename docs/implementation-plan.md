# Wavetable Designer: implementation plan

Status: C++20 with native JUCE selected; scaffold, freehand drawing and smoothing implemented on
feat/wavetable-designer-scaffold; audio preview is implemented for Windows review. Windows/macOS, lengths, preview and persistence
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
samples share screen pixels, so interpolated drawing is essential. Horizontal zoom
and Sample view provide precise index selection and drawing without changing data.

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
commit: 2bd9da8. Drawing was user-validated and committed as c8ed303. Whole-cycle and selected-region smoothing
are subsequent user-approved changes included in the editor commit milestone below.

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
4. Drawing/model/undo/presets and periodic smoothing implemented.
   Completion: skipped/reversed strokes and boundary smoothing are verified;
   slider adjustments do not accumulate and undo restores exact prior samples.
5. Implemented audio preview and device settings on Windows (moved forward at
   the user's request); user audition and native macOS checks remain.
   Completion: offline checks verify note frequency, wrapping, update transitions,
   callback memory behavior and harmonic limiting; real output callbacks and
   manual play-stop/editing checks show stable output.
6. Implement project save/open and export.
   Completion: project round-trip restores authored data. An independent reader
   verifies WAV format/rate/frame count, raw byte count/endianness and sample
   agreement. Verify SZ404 whole-cycle resampling, quantization and actual WAV/raw
   import paths with golden cases.
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

## Smoothing implementation and validation (2026-10-13)

Drawing was confirmed working by the user and committed as c8ed303 before this
stage. Added a collapsible Smooth panel above the canvas. The original curve is
shown faintly behind the preview. Other sample edits are paused until Apply or
Cancel; Enter applies, and Escape or Undo cancels. Minimum window size is now
640x480 to accommodate the expanded controls while retaining a useful canvas.

The numerical operation is a positive, normalized circular Gaussian convolution
blended with the original by strength a in [0,1]. For the supported lengths, maximum
sigma is N/32 (32 samples at 1024, 64 at 2048); larger custom/headless cycles cap it
at 64. Sigma = 0.5 + a^2 * (maximumSigma - 0.5). The kernel extends to ceil(4*sigma)
on either side. Output = (1-a)*source + a*convolution(source). Zero strength returns
an exact copy. The positive normalized kernel preserves DC and stays within the
input amplitude range; peaks may decrease and are never renormalized.

Every preview reads the captured original State; previews do not accumulate.
Apply adds one snapshot to the bounded undo history, and Cancel restores source
samples/preset identity. Cancel and no-op Apply preserve the redo branch. A pending
preview cannot be mixed with drawing, preset replacement or length changes.

Windows validation: the core suite passed periodic seam symmetry, cyclic-shift
equivalence, constants/DC/bounds, reduction of rapid variation, matching phase
width at 1024/2048, invalid inputs, stable repeated previews, and exact history
behavior. GUI checks passed preview controls, Apply/Cancel, Enter/Escape/Undo,
blocking other edits, and re-enabling them afterward. Both themes were visually
inspected, including 640x480. macOS native validation remains outstanding.

The review app uses build/app-smoothing-windows, leaving the existing live drawing
session intact. tools/dev.py supports --build-dir for isolated app/core review
caches and reuses locally fetched sources when available. The standard product
build remains self-contained through pinned FetchContent declarations.

## Selected-region smoothing refinement (2026-10-13)

User feedback identified an uneven sloping section bounded by intentional sharp
jumps. Added selection smoothing to preserve those jumps while cleaning up the
section, alongside the existing whole-cycle operation.

The initial refinement exposed Select region as a canvas tool and Shift-drag shortcut
(superseded by the contextual controls below). Horizontal
drags select inclusive nearest-sample endpoints regardless of vertical position,
in either direction, with boundary clipping. Selection shading and endpoint dots
show the region. Clear selection removes it. Selection gestures never change
samples or create waveform undo actions; Escape/Undo during a gesture restores the
prior range. The region is retained in edit snapshots and remapped on length changes.
Selections are one contiguous interval and do not currently span the phase seam.

Smooth defaults to Selected region when a range exists, otherwise Whole cycle.
The preview target menu can switch scopes while always reading the captured source.
The initial refinement paused range changes during preview (superseded below).
Apply/Cancel retain the existing single
undo/non-cumulative behavior. Missing or tiny selections never silently fall back
to smoothing the entire cycle; fewer than three samples is a no-op.

Regional smoothing fixes both endpoint samples and copies every outside sample
unchanged. Subtract the straight line joining the selected endpoints, smooth the
residual using a Gaussian kernel sized to the selected interval, then add the
trend back. The residual is zero beyond the interval; no outside waveform values
are read. Float-quantized straight slopes within two float epsilons are returned
unchanged. This prevents hard boundary jumps from bleeding into the selection.
The result remains finite and amplitude-bounded; it does not normalize peaks or
force cycle endpoint equality. Whole-cycle smoothing retains its DC-preserving
property; regional endpoint anchoring does not enforce a global DC constraint.

Windows checks passed: unchanged endpoints/outside samples and jump sizes,
no cross-boundary influence, removal of alternating noise on a slope, straight-line
and tiny-range no-ops, selection clipping/validation, selection history, stable
scope switching, and the full drawing/smoothing regression suite. Hidden native
handler checks passed checkbox/Shift-drag selection, reverse drags, cancellation,
Undo during a selection gesture, scope changes, Apply/undo/redo and Clear selection.
Dark/light snapshots were visually inspected at both sample lengths and 640x480.
Native macOS verification remains outstanding.

Review build: build/app-regional-smoothing-windows. Existing app sessions are not
closed during this build. No new commit was made for this feedback refinement.

At this stage the next milestone was save/open and export; the user subsequently
moved audio preview ahead of them (see the audio milestone below).

## Contextual selection and horizontal zoom (2026-10-13)

User feedback moved region selection into Smooth rather than the regular drawing
toolbar. The initial target menu (superseded by the checkbox below) exposed
Select region only after opening Smooth.
The canvas remains interactive for selecting/panning but blocks waveform drawing
until Apply or Cancel. A nearest-sample vertical guide and zero-based index label
follow the cursor. The shaded interval has draggable vertical boundary handles;
either endpoint can be moved and is clamped to the other endpoint if crossed.

Range creation/adjustment immediately recomputes from the captured waveform,
never from the displayed preview. No range shows the exact source while waiting
for a drag. Apply/Enter requires at least three selected samples; an empty range
cannot silently apply whole-cycle smoothing. Clear selection starts a fresh range.
Escape/Undo during a gesture restores the previous range and preview. Outside a
gesture, Escape/Undo cancels the transaction, restoring the source and pre-preview
selection. Apply is still one waveform undo action with before/after metadata.

A JUCE-free HorizontalViewport provides a shared phase transform for rendering,
drawing, region creation, handles and hover guides. The header offers Whole cycle,
2x–64x, and Sample view. Zooming from the complete cycle centers on a selection
when present; subsequent zoom changes preserve the viewport center. The native
horizontal scrollbar navigates within phase [0,1]. Sample view shows sample grid
lines/points at a minimum of 12 pixels per sample and adapts to window/length
changes. Drags beyond the plot clamp to visible bounds. Panning and zooming are
view changes only: no resampling, waveform history, or export changes.

Review build: build/app-selection-zoom-windows. Existing native sessions remain
open. Windows verification passed 34 core cases with 48,387 assertions covering
the source transaction, selection history and viewport coordinate round trips.
The hidden native handler suite passed contextual selection, hover guides,
reverse creation, both boundary drags, crossing clamps, gesture cancellation,
missing-range Apply protection, real scrollbar callbacks, exact one-sample
adjustment/drawing, zoomed edge clipping, length/undo adaptation and existing
drawing/smoothing regressions at 1024/2048 and 640x480. Dark/light full-cycle and
sample-view snapshots were visually inspected. git diff --check passed.
Native macOS validation remains outstanding. These changes were reviewed before the editor commit milestone below.

## Scope checkbox and angle-based edge preservation (2026-10-13)

The previous refinement replaces the smoothing target dropdown with a Select
region checkbox. Checked selects region smoothing; unchecked selects whole-cycle
smoothing. Existing adjustable ranges, cursor guides and zoom remain available.
Preserve edges is a second checkbox inside Smooth, initially off. When enabled,
an Edge angle slider initially appeared alongside Smoothness (rearranged below): 1–180 degrees, default 30.
Apply/Cancel replace Clear/Smooth in the toolbar during preview, keeping the canvas
height unchanged even at 640x480.

Define a corner by absolute direction change between fitted incoming and outgoing
tangents in normalized geometry: x = phase, y = amplitude/2. A straight line has
zero turn regardless of its steepness. Least-squares fits cover N/128 samples on
each side (8 at 1024, 16 at 2048; clipped at selection endpoints). This reduces
sensitivity to individual sample wiggles and keeps the definition independent of
viewport size/zoom and approximately consistent between cycle lengths. Very small
features can be smoothed; the threshold controls sensitivity. The test includes
a known 30-degree turn that is retained at 25 and rounded at 35 for both lengths.

Samples above the angle threshold become fixed anchors. Smooth each interval's
residual from its endpoint-to-endpoint line, using the existing anchored Gaussian
method. This keeps the detected points exact and avoids averaging across sharp
boundaries, rather than only restoring individual points after a global filter.
Whole-cycle intervals wrap through the phase seam. With no detected edges, retain
the original circular smoothing behavior. In a selected range, both the fit and
smoothing stay entirely inside the selection, and its original endpoints/outside
samples remain exact. Edge-preserving smoothing does not impose global DC
preservation or normalization; ordinary circular smoothing still preserves DC.

Checkbox/angle/strength/range changes always read the same captured source.
Apply remains one undo action; Cancel restores source samples and pre-preview
selection. Review build: build/app-preserve-edges-windows, leaving prior sessions
open. Windows verification passed 41 core cases / 60,747 assertions, including
standard-shape corners, both sides of step jumps, reduction of small alternating
noise between edges, threshold sensitivity at both lengths, cyclic-shift/polarity
invariance, selected-boundary isolation, invalid inputs/tiny cycles, and exact
preview/apply/cancel history. Native smoke checks passed checkbox scope switching,
conditional angle slider/default, repeated angle changes from the source, both
scopes, Apply/undo/redo/Cancel and the existing drawing/selection/zoom regressions
at 1024/2048 and 640x480. Both themes were visually inspected with edge preservation
on/off and with/without a region. git diff --check passed. macOS native validation
remains outstanding. These changes were reviewed before the editor commit milestone below.

## Smoothing layout and optional jitter cleanup (2026-10-13)

Latest user feedback places Edge angle on the same row as Preserve edges, moves
selection indices beneath Select region, leaves that left area empty when unchecked,
and places Smoothness to its right. Clear is a compact selection-context button
beside the indices. Reduce jitter initially appeared only with Preserve edges
(superseded by the independent option below). The canvas top is 252 during Smooth, a small adjustment
from 244; enabling/disabling options does not change its height. Regular drawing
layout remains unchanged.

A likely source of the reported jagged remnants is the previous detector: every
above-threshold sample became a fixed anchor, freezing whole neighborhoods around
corners and creating tiny smoothing intervals. Optional Reduce jitter keeps the
accepted default behavior available while introducing more selective detection.
Require the same signed turn to exceed Edge angle at both N/128 and N/64 scales;
keep local peaks of the coarse angle within the first window. Both sides of an
isolated hard step are retained when their angles qualify (jump > 0.04 amplitude,
more than eight times the neighboring differences). Selection endpoints always
remain fixed. Neither detection step reads outside the selected range.

Between anchors, try a constrained quadratic:
y(t) = lerp(first, last, t) + A*t*(1-t), 0 <= t <= 1.
Least squares determines A; curvature comparable to residual noise (|A| <= 4*RMS)
uses the endpoint-to-endpoint straight line instead. Accept only when RMS error
<= 0.02 amplitude, maximum error <= 0.04, and the fitted endpoint tangent turn
<= Edge angle. Full Smoothness uses the accepted fit; otherwise blend from the
original source by strength. Already-clean lines/quadratics within float tolerance
remain exact no-ops. Complex or poorly matching spans use the original anchored
Gaussian. With no whole-cycle corner anchors, circular Gaussian remains the default.

This is a deliberately conservative first tolerance and an optional editing tool;
small intentional detail within that tolerance can be removed. The original stays
visible in preview, and Apply/Cancel preserve existing transaction behavior.
A later refinement can expose fit tolerance if the user needs finer control.

Polynomial smoothing is a useful reference for retaining simple curve geometry;
[SciPy's primary Savitzky–Golay documentation](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.savgol_filter.html)
demonstrates preserving quadratic samples. This implementation uses a constrained
fit across an eligible interval, rather than the standard sliding filter, and adds
no SciPy/Python runtime dependency.

Review build: build/app-jitter-cleanup-windows, preserving existing sessions.
Core validation passed 47 cases / 75,137 assertions: straight ramps become lines,
shallow curves retain curvature, zero/half/full strength, clean no-ops, Gaussian
fallback, protected standard shapes/steps, selection isolation, cyclic shifts,
tiny cycles, stable checkbox previews and exact undo/redo/cancel. Native checks
passed option visibility and independence, strength/toggle replay from one source,
whole/selected cleanup, blank inactive range controls, sample view/panning,
Apply/undo/redo/Cancel and the full drawing/smoothing/selection regressions at both
lengths and 640x480. Dark/light snapshots were visually inspected, including the
aligned checkbox/slider rows, range layout and cleanup at individual sample scale.
git diff --check passed. macOS native checks remain outstanding. These changes were reviewed before
the editor commit milestone below.

## Independent Reduce jitter option (2026-10-13)

The user requested separating Reduce jitter from Preserve edges. It is now visible
throughout the Smooth context and can be enabled alone or combined with edge
preservation. Smoothness still controls the blend; region selection, Apply/Cancel
and one-action history remain shared preview controls. The layout is unchanged.

Standalone jitter cleanup attempts the conservative line/quadratic fit within the
selected range or the authored sample interval 0..N-1 for Whole cycle. Accepted
fits keep those original endpoints and use a fixed 30-degree gentle-turn guard.
The hidden Edge angle value is irrelevant, including core validation of a disabled
angle parameter. A poorly matching selected span falls back to anchored Gaussian;
a poorly matching whole cycle falls back to circular Gaussian. Standalone cleanup
does not detect or freeze interior corners. Enabling Preserve edges adds the
existing fixed corner intervals and makes its angle setting govern the fit.
A whole cycle with no detected corners can also attempt the gentle fit when
jitter reduction is on.

Review build: build/app-independent-jitter-windows. Existing sessions are preserved.
Core and native checks cover standalone whole/selected fitting, all four checkbox
combinations, independence from hidden angle settings, source-based previews and
exact apply/history/cancel restoration. Windows core verification passed 49 cases /
84,401 assertions. The native regression suite passed at 1024/2048 and 640x480;
standalone jitter snapshots in both themes were visually inspected. git diff
--check passed. macOS native verification remains outstanding. These changes were reviewed before
the editor commit milestone below.

## Editor commit milestone (2026-10-13)

The user approved committing and pushing the reviewed editor changes on
feat/wavetable-designer-scaffold. This milestone includes whole-cycle and regional
smoothing, contextual adjustable ranges, sample-level zoom/panning, angle-based
edge preservation, independent jitter cleanup, native regression checks, and
isolated review builds that preserve unsaved sessions. Windows validation passed
49 core cases / 84,401 assertions and the native GUI/theme suite; macOS validation
remains outstanding. Generated build output and snapshots stay ignored.

## Audio preview milestone (2026-10-13)

The user approved implementing playback before file persistence/export. The
native footer adds Play/Stop, C1–C8 notes (A4/440 Hz by default), independent
volume (20% initially), Audio settings and the existing cycle length. Space toggles
transport from the canvas. No device is opened at startup; Play/Audio settings
lazily opens the system output with zero inputs. Device type/output and advanced
rate/buffer settings use JUCE's native selector. Device restarts stop transport.
Settings remain session-only until the persistence milestone.

Portable PreviewBank uses a radix-2 FFT on an immutable authored snapshot and
constructs ten 2048-point tables, with highest harmonics 1023, 511, ..., 3, 1.
Playback chooses the richest level below 0.45 * device sample rate / note frequency.
The oscillator wraps phase using note frequency/device rate and linearly reads the
table. Notes above the fundamental guard are silent. Preview removes DC and scales
reconstruction overshoot only; authored data is unchanged and quiet waveforms
are not boosted. Output is mono duplicated into enabled output channels, with
maximum gain 0.25 times the volume setting.

Publication ownership was reviewed before implementing the callback: four fixed
banks cover two consumer voices, one ready bank and a producer write. A CAS claims
only free slots. The producer copies a finished bank, release-publishes its index
and reclaims any replaced ready slot. The consumer acquire-takes that index only
between crossfades and holds old/new voices for 10 ms, then release-frees the old
bank. Pitch swaps retain the old voice's frequency/table while fading to a new
voice, preserving each voice's harmonic limit. The callback has no allocations,
blocking locks, shared_ptr destruction, file I/O or dynamic bank reclamation.
Sample-rate restarts reset ramps/phase while retaining the current bank safely.

A worker coalesces pending GUI snapshots and builds outside the audio callback.
Only numerical waveform changes publish, including live drawing, undo, resampling
and pending smoothing/Cancel. Control values are lock-free atomics. Transport and
volume use 20 ms ramps; note/table changes use 10 ms crossfades. Worker exceptions
and output errors surface in the footer. A settings window is owned by the editor
and destroyed before its referenced device manager/look-and-feel.

Verification: 56 core cases / 142,576 assertions passed, including A4 at both cycle
lengths and 44.1/48/96 kHz, spectral exclusion/DC/bounds, low-rate high-note muting,
null outputs/stereo duplication, fade-to-zero, pitch swaps, sample-rate restarts,
zero allocations/deallocations through callback updates and overlapping producer/
consumer publication. Core-only builds remain JUCE-free. JUCE audio_utils supplies
the selector and native output adapter; ASIO SDK support is disabled.

Native smoke uses an offline device mode and never emits audio. The separate
audio-check command emits a 1.5-second quiet A4 tone and reports driver rate,
buffer size, rendered frames and peak in build/audio-check/device-check.json.
It checks callback operation, not acoustic output quality. Windows native smoke
passed offline transport, note/volume changes, live smoothing/Cancel, Clear/undo,
worker revision coalescing and source restoration at both lengths and 640x480.
Both editor themes and output-selector snapshots were visually inspected; smoke
also asserts no physical device opens. The separate Realtek Speakers check passed
at 48 kHz / 480 frames per buffer, with 65,280 rendered frames and peak 0.02.
These verify the code and driver callback path. The user confirmed playback works
on Windows; native macOS validation remains outstanding. git diff --check passed.
Review build: build/app-audio-preview-windows.
The user confirmed playback works and approved a commit and PR. Audio changes are
on feat/audio-preview; the PR targets the current default branch,
feat/wavetable-designer-scaffold. Existing live sessions remain open.
