# TONIC-inspired V11 — Audio file drag-and-drop

Windows VST3 JUCE project, continuing the compact **piano-free** analyzer.

## New in V11
- When the host is not providing audible input, drop a supported local audio file **anywhere on the plugin UI**.
- An **Open Audio** button gives a fallback when a host prevents file drag-and-drop.
- Runs file decoding, FFT key analysis, chord preview and BPM estimation off the real-time audio thread.
- Shows filename, analysis progress and decoding errors.
- **Live Input** returns to analysis of the incoming DAW signal.
- Handles WAV, AIFF, FLAC, OGG and MP3 when supported by the linked JUCE audio format decoders.
- Uses source file sample rate for analysis (no destructive resampling of source files).
- Caps long files at 12 minutes of analyzed audio to avoid unbounded processing.
- While File mode is active, live audio still passes through unchanged but is not used for detection.

## Behavior
- No input means silence measured on the plugin's input channels; the input meter clears after 24 silent audio callbacks. Some DAWs may not deliver callbacks while transport is stopped.
- Dropping a file while audible audio is arriving is intentionally disabled; stop or mute the source first.
- This loads a **local file** into analysis only; it never overwrites source media, and it does not generate sound.
- File analysis is not guaranteed to work in all DAWs because operating systems/hosts may block drag-and-drop into plugin windows. Use Open Audio in that case.
- BPM is heuristic and can produce double/half-time errors; chords are experimental.

## Build on GitHub
Upload contents of ZIP to repository root, then run `.github/workflows/build.yml` on Windows.
Locally: `cmake -S . -B build -A x64` then `cmake --build build --config Release`.
JUCE 8 is fetched by CMake if `JUCE_PATH` is not specified.

**Status:** Source has not been Windows compiled or DAW-tested in this environment. Validate on GitHub Actions and then in FL Studio.


## V12 reliability pass
- File requests use generation counters to prevent a canceled/older scan from reporting success or overwriting the next scan's status.
- Live-input detection uses elapsed samples (350 ms silence) rather than an arbitrary number of audio blocks.
- File progress now reports analyzed seconds and duration, plus a progress line under the status.
- No piano keyboard; drag/drop anywhere still supported when no audible live input is present.
- Fixed UI messaging after a completed file that contains no detectable key.

**Known limitations:** file analysis still supports only formats enabled in the JUCE build; decoder support varies. This is independent reimplementation, not binary-equivalent to the commercial TONIC plugin. Build in GitHub Actions and test within FL Studio before release.


## V13 performance/accuracy pass
- Fixed undefined `silentAudioBlocks` reference; live-input detection now uses elapsed samples.
- Precomputed Hann window (no per-frame trigonometric window generation).
- BPM: 40–240 range, normalized lag scoring with octave-related beat candidates, continuity prior, optional weak DAW prior. Confidence is lowered when half/double-time hypotheses compete.
- Chords: uses a faster-adapting chroma window rather than the long-term key profile, penalizes out-of-chord energy, imposes a minimum two-window stability requirement, and abstains on uncertain mixtures.
- Tuning: log-magnitude parabolic FFT-bin interpolation and smoothed cent estimates.
- These are heuristics, not ground-truth transcription or a replica of original TONIC DSP.
- GitHub build and FL Studio functionality have NOT been verified locally.

## V14: automatic Reference A (no knob)
- Removed manual reference tuning parameter and its rotary control.
- Reference A is estimated automatically from spectral peaks against A440.
- Shows detected Hz and cents offset; waits for usable pitch evidence.
- Cached MIDI/harmonic FFT-bin lookups per sample rate and spectral powers per FFT frame.
- NOTE: Former referenceHz preset automation is no longer available.
- This source update is not verified by a Windows VST3 compilation or accuracy benchmark.


## V15 detector maintenance
- Offline sample-rate fix: rebuild cached FFT-bin mapping when a file sample rate differs from the live DAW rate, and restore it when returning to live mode.
- More efficient analysis: cache note frequencies and the onset hop size instead of recalculating them for every FFT window/audio sample.
- Guard FFT-bin indexing for low-rate files, avoiding invalid spectral access.
- Reference A remains **automatic only**. A minimum number of spectral tuning observations and a weighted effective-peak confidence are used to suppress some spurious readings.
- Piano remains removed. GitHub Actions workflow unchanged.

**Validation:** source-level assertions and ZIP integrity checked. Windows VST3 build, numerical benchmark, and FL Studio accuracy testing have not been performed.
