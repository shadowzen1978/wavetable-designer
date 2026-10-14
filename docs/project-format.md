# Project and export formats

## Version 1 projects

A .wtd file is UTF-8 JSON. The sample array is the authored cycle; no preview gain,
DC removal, harmonic filtering, normalization or integer quantization is applied.

~~~json
{
  "format": "wavetable-designer",
  "version": 1,
  "sampleCount": 1024,
  "samples": ["... exactly 1024 numeric amplitudes ..."],
  "preset": null,
  "selection": { "first": 128, "last": 512 }
}
~~~

The sample entry above is an explanatory placeholder, not a valid project array.
All six fields are required. sampleCount is integer 1024 or 2048; samples contains
exactly that many finite numbers in [-1, 1], at phases i/N. There is no phase-1
duplicate. Values are float32 in the editor and written with enough decimal
precision to round-trip exactly, including subnormal values and negative zero.

preset is null for a custom drawing, or one of sine, triangle, saw, square, flat.
It describes editor identity; loading never regenerates samples from the name.
selection is null or an object with inclusive integer first/last indices satisfying
0 <= first <= last < N. The region need not be active when reopened.

Unknown fields are ignored. Future version numbers are rejected rather than
interpreted as version 1. Readers reject missing/incorrectly typed fields,
inconsistent counts, invalid amplitudes/ranges, unbalanced/trailing JSON data,
files above 1 MiB, and nesting beyond 16 levels.

Open parses and validates the entire file before replacing the editor. It restores
samples, preset identity and selection, clears undo/redo, resets the view to the
whole cycle and stops playback. New creates an untitled 1024-sample sine cycle.
Save does not clear history. Dirty state compares the current samples/preset/range
with the last saved/opened checkpoint, so Undo back to it removes the asterisk.

Projects exclude undo history, temporary smoothing previews, zoom, theme, preview
note/volume, device configuration and export controls. Those controls remain
session preferences in this milestone. Apply or cancel smoothing before saving,
opening, creating a project or exporting. Closing during a preview first offers
Apply, Discard preview or Cancel, followed by the normal unsaved-project prompt.

File commands use asynchronous native choosers and guarded callbacks. New/Open/Quit
offer Save, Discard or Cancel when dirty. Save failure or chooser cancellation
prevents the pending destructive action. Editing is paused while file dialogs run.
Save As/Export enforce the appropriate extension before asking to replace an
existing destination. Ordinary Save updates the associated project directly.

Writers create a temporary file beside the destination, check the complete write
and flush, then replace the target. Failure leaves the editor/checkpoint unchanged.
Windows locked-target failure is covered by native smoke. This is replacement
protection, not a guarantee against power loss or every filesystem failure.

## Exports

The footer selects profile, encoding/container and WAV header rate. Cycle length
still controls the authored document and is an undoable resampling edit. The
summary reports the actual exported count and any SZ404 conversion.

| Profile | Count | Encodings | Containers |
| --- | --- | --- | --- |
| SZ404 (default) | Exactly 1024 | PCM16 | WAV or raw |
| Generic | Authored 1024 or 2048 | PCM16 or IEEE float32 | WAV or raw |

WAV is mono RIFF/WAVE, with 44.1 or 48 kHz header metadata. PCM uses format tag 1,
16 bits, and 2-byte block alignment. Float uses tag 3, 32 bits, 4-byte alignment
and a fact chunk containing the frame count. Header rate never changes samples,
count, preview pitch or output-device settings.

Raw is explicitly little endian, with no prefix, sample-rate metadata or header.
PCM16 raw is N*2 bytes; float32 raw is N*4 bytes. SZ404 raw is exactly 2048 bytes.

PCM quantization uses round-half-away-from-zero(value * 32767):
-1 maps to -32767, zero to 0, and +1 to 32767. Float exports retain exact float32
sample bits. Export does not alter project samples/history/dirty state.

For SZ404, an authored 2048 cycle is periodically linearly resampled to a separate
1024 copy, covering the entire cycle. It is never truncated to its first half.
Generic exports keep the authored count. Listening volume, DC removal and harmonic
limiting affect only the preview.

## Independent and consumer checks

Run mise run smoke first to generate real product files under build/gui-smoke/files,
then mise run check-files. NumPy/libsndfile independently validate all 24 supported
profile/format/rate/length combinations and compare every exported sample against
the .wtd data, including symmetric PCM quantization and complete-cycle resampling.

For an individual file, add --project and --profile to the existing checker:

~~~sh
mise run check-exports -- waveform.wav --samples 1024 --encoding pcm16 --rate 44100 --project drawing.wtd --profile sz404
~~~

The optional Windows consumer check links already-built Release libraries from
the sister repo and calls SZ404AudioProcessor::loadWavetableFile directly:

~~~sh
mise exec -- python tools/check_sz404_import.py
~~~

Use --repo/--sz404-build to select another checkout/build. This does not build or
modify the sister repository, open its editor/audio device, or add a product
dependency. The reference headers must match the prebuilt x64 MSVC Release libs.

Windows review passed 12 actual SZ404 imports: PCM16 WAV/raw from both authored
lengths at both rates, plus generic float WAV. The PCM WAV path decodes integers
with a /32768 scale and then quantizes again with *32767; its stored value can
differ from raw by one integer step. Float WAV quantizes once. The harness checks
those actual consumer semantics, including its truncation of generic 2048 WAV.
Use the SZ404 profile for a complete cycle in that synth.

Native macOS file choosers, replacement behavior and runtime checks remain pending.
