# Language/frontend comparison and compatibility findings

Date: 2026-10-13. Status: historical comparison. C++20/native JUCE selected after this review.

## Native Python/PySide6 is viable

For a 1024/2048-sample editor, Python is sufficient for waveform manipulation,
undo snapshots, persistence, and export. NumPy is a convenient implementation for
circular smoothing, periodic resampling, FFT analysis and harmonic-limited tables.
Use a custom QWidget/QPainter canvas with Qt controls, commands, dialogs and themes.
Keep arrays/model math separate from Qt so numerical tests run without a GUI.

Recommended Python candidate dependencies: PySide6, NumPy, soundfile, and pytest
for tests. Start with QtMultimedia QAudioSink for buffered monophonic preview;
prototype continuous refill, note changes, table transitions and device switching
before accepting the implementation. Generate chunks with vectorized operations
outside the device audio thread. Qt's QIODevice output mode is a possible buffered
path; modest latency is acceptable for an editor preview. Do not assume Python
callbacks meet a hard realtime contract or call UI/model methods from them.
If a measured issue requires native audio, reassess rather than automatically
adding C++/JUCE to a second framework. Finite pre-rendered audition is simpler but
is not an equivalent substitute for agreed continuous oscillator preview.

Python/uv helps implementation speed and consistency with current experiments.
No CMake harness is needed for a pure Python app; use uv and pytest. Deploy native
Windows/macOS artifacts with a pinned, verified packaging path such as
pyside6-deploy/Nuitka. Each OS still needs native build and device tests. Packaging
and Qt plugin inclusion are real work, not reasons to dismiss Python.

Primary docs: [QAudioSink](https://doc.qt.io/qtforpython-6/PySide6/QtMultimedia/QAudioSink.html)
and [pyside6-deploy](https://doc.qt.io/qtforpython-6/deployment/deployment-pyside6-deploy.html).
Check the exact pinned PySide version's supported APIs during the preview spike;
latest generated docs include C++ snippets that may not directly apply to Python.

## Browser integration depends on frontend architecture

| Desktop UI | Reusable in browser later | Work remaining |
| --- | --- | --- |
| Native JUCE | Model/schema, fixtures, math specification; potentially compiled core | HTML/canvas UI, browser audio/file adapters |
| Native PySide6 widgets | Model/schema, fixtures, math specification | HTML/canvas UI and JS math/audio implementation |
| Native C++ Qt widgets | Model/schema, fixtures, potentially compiled core | Browser-friendly UI/audio/file adapters |
| HTML/JS inside Python or C++ Qt | Much of UI and JS math | Host adapter replacement and browser deployment/behavior QA |

A Python backend is easy to connect to browser prototypes over a small local API,
but that does not by itself produce an offline browser app. A later browser product
should work locally with JS/TypeScript and Web Audio, without requiring a hosted
Python server for waveform edits. Use common input/output fixtures and a versioned
project schema to verify equivalent math across implementations.

Qt supports embedded HTML with QWebEngineView and QObject/JavaScript interaction
through QWebChannel. Both Python and C++ can use this architecture; Python has no
exclusive browser-integration advantage. WebEngine is Chromium-based and brings
additional runtime/packaging work, including macOS helper signing. The user rejects
Electron; an embedded Chromium approach is a separate choice, not implicitly agreed.

Sources: [QWebEngineView](https://doc.qt.io/qtforpython-6/PySide6/QtWebEngineWidgets/QWebEngineView.html),
[QWebChannel](https://doc.qt.io/qtforpython-6/PySide6/QtWebChannel/QWebChannel.html),
[Qt WebEngine overview](https://doc.qt.io/qt-6/qtwebengine-overview.html),
[WebEngine deployment](https://doc.qt.io/QT-6/qtwebengine-deploying.html).

For native C++/JUCE, keep core types and operations independent from JUCE,
threads, devices, filesystem and windows. Emscripten can compile suitable C++ code
and expose it to JavaScript; a small browser core could reuse those operations.
This is a future feasibility spike, not a promise that JUCE UI or native audio
code ports automatically. For these small arrays, a tested JS port may be simpler.
[Emscripten integration documentation](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/index.html).

Browser mockups remain useful with any native stack: reuse interaction decisions,
layout, colors and fixtures, then implement native components. Avoid paying for
simultaneous production desktop/browser UIs before desktop behavior stabilizes.

## LyricPrompter2 reference

Inspected C:/dev/LyricPrompter2/LyricPrompter.pro, src/main.cpp, src/testwidget.h,
and relevant portions of src/testwidget.cpp. It is C++ Qt, with QApplication,
QWidget, QPainter, QFontMetrics, paintEvent and keyboard events. It uses qmake,
QtGui umbrella includes, a custom INI reader, and a MacOSX10.6.sdk path.

Native PySide6 widget/event/painting concepts translate well to this project.
Native C++ Qt is closest in language and APIs if revisiting that app is a major
priority. Reusing its old SDK/build configuration is inappropriate for a new app.
Modernizing LyricPrompter2 itself is outside this repository's scope; no changes
were made to it. C++ Qt6 + QAudioSink is a credible third choice, but JUCE remains
more directly aligned with existing SZ404 audio/build work.

## SZ404 export evidence

Inspected current local sources, not old reverse-engineering assumptions:
- [Manual](../../ts404-remake/manual/user-manual.md), user wavetable section, line 241.
- [WAV importer](../../ts404-remake/plugin/Source/app/PluginProcessor.cpp), loadWavetableFile, line 891.
- [Raw importer](../../ts404-remake/plugin/Source/app/SessionState.cpp), overlayRawWavetable, line 116.
- [Table definition](../../ts404-remake/plugin/Source/dsp/Oscillator.h), kWavetableSize = 1024, line 26.
- [Existing tests](../../ts404-remake/plugin/Tests/UserWavetableLinkTests.cpp).
- Cached JUCE WAV reader in build/_deps/juce-src supports float32 and PCM32 reads.

WAV: first 1024 frames only, averaged to mono, amplitude clamped and stored with
lround(v * 32767) in int16. Sample-rate metadata does not influence table pitch.
Short WAV input gets zero padding. Longer input is truncated, not resampled.
Raw: first 1024 signed 16-bit little-endian samples, no header/prefix. Short raw
input leaves unmodified table samples beyond its whole-sample payload.

Recommendation: a named SZ404 profile exports a complete 1024-sample cycle as
PCM16 mono WAV (44.1 kHz default) or PCM16 LE raw (2048 bytes). It resamples a copy
if the authoring cycle has 2048 samples. A generic profile retains float32 WAV and
1024/2048 plus 44.1/48 kHz choices. Do not label float32 raw as SZ404-compatible.

There is no single universal wavetable-device encoding/length. Xfer's Serum 2
manual documents mono 16-bit and 32-bit WAV at 44.1 kHz, 2048 samples per subtable.
That supports a 2048/44.1 kHz software profile, but the cited manual does not specify
the 32-bit subtype; float32 is our proposed authoring/export choice. Elektron's
Digitakt factory samples use mono 16-bit/48 kHz, while Waldorf's Blofeld format has
a different frame structure. Named verified profiles are clearer than universal
compatibility claims. Keep multi-frame/proprietary formats outside the first MVP.

Sources: [Serum 2 manual](https://www.xferrecords.com/manual/serum-2/docs),
[Elektron factory sample format](https://support.elektron.se/support/solutions/articles/43000570070-digitakt-missing-factory-content-),
[Waldorf user wavetables](https://waldorfmusic.com/faq-items/user-wavetables/).

Implementation validation must inspect WAV subtype, length/rate, decoded levels,
raw payload/endianness, and target quantization. Exercise SZ404's actual import
paths with representative generated files, not only our own reader. Pay attention
to half-step rounding: NumPy's default ties-to-even is not C++ lround's rule.
Allow for decoder scaling when comparing PCM WAV imports versus direct raw; do not
assume the two paths preserve identical integer samples without measuring them.
No export/import runtime checks have been performed during this planning phase.
