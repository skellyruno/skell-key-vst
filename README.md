# Skell Key V55 — Lean GitHub VST3 source

This is the **minimal build package** from V54. V55 builds on V54 with a more accurate key/tuning/tempo analysis and a redesigned skeleton-key UI. Key, chord, BPM and Reference A detection and priority drag-and-drop analysis are all retained.

## Build on GitHub

1. Put **the contents of this ZIP directly in the repository root**, not inside an additional folder. Make sure `.github/workflows/build.yml` is committed.
2. Open **Actions → Build Skell Key Windows VST3 → Run workflow**, or push to `main`.
3. After a successful build, download `Skell-Key-Windows-VST3` from the workflow's Artifacts section.

CMake downloads JUCE 8.0.12 during configuration. For a local JUCE checkout, configure with `-DJUCE_PATH=/path/to/JUCE` instead.

## Contents

- `Source/` — the 22 source and header files required by the plugin.
- `CMakeLists.txt` — JUCE 8 CMake project, VST3 format.
- `.github/workflows/build.yml` — Windows x64 GitHub Actions build and artifact packaging.
- `.gitignore` — keeps build output out of source control.

## Excluded deliberately

Historical benchmarks and logs, obsolete README/change files, old docs, standalone benchmark harnesses, Python training scripts, test files, generated artifacts, and the separate experimental model-training CI workflow. None is required to compile the VST3. Existing model-weight headers **are kept** because source headers depend on them, even though the learned model remains disabled until validated.

## What changed in V55

**Accuracy** (`PluginProcessor.cpp`)
- **Key is decided from long-term evidence.** The ranking used a ~0.5 s chroma memory, i.e. roughly the current chord. It now uses a ~20 s memory (running average until that much audio has been heard; scaled by the *responsiveness* parameter). The short memory still drives chords and the on-screen spine.
- **Peak-based note evidence.** Spectral peaks are interpolated and credited to the nearest note(s) with a cosine taper, instead of each note reading the maximum of ±2 bins around its 440-grid bin (which let neighbouring semitones share one bass peak and missed off-grid treble peaks).
- **Tuning estimate covers the full ±50 cents** (A=432 Hz is −31.8 cents; the old estimator rejected anything beyond ±30) using the mode of a deviation histogram, which ignores non-equal-tempered overtones. The tuned grid is then used for the chroma, so detuned or sampled material is analysed correctly.
- **Tempo prior strengthened** (log-normal around 120 BPM, weight 0.40, was 0.08) to reduce half-time answers.

**UI** — skull-bowed skeleton-key mark, bone-on-black palette with one ecto-green accent, a live 12-vertebra pitch-class "spine" with the tonic and scale tones highlighted, key-bit "teeth" confidence meters, a cents gauge for Reference A, and a resizable window (fixed aspect, 480 × 304 up to 1200 × 760). The `preferFlats` parameter is now honoured in the display.

## Measured results (synthetic, headless)

48 generated tracks × 6 timbre/tuning/drum-style sets, each with known key, BPM and tuning (tuning offsets −35…+35 cents, 72–160 BPM, four drum styles):

| | V54 | V55 |
|---|---|---|
| Key exact | 69 / 288 (24 %) | 216 / 288 (75 %) |
| Tuning, mean abs error | ~12 cents | 0.2 cents |
| Tempo exact (±2.5 %) | 10 / 263 | ~32 / 263 |

Key results are also stable across 22.05 / 44.1 / 48 / 96 kHz; V54 flipped keys between sample rates.

## Limitations

- These numbers come from **synthetic music**, not real recordings, so treat them as evidence of direction, not as a guarantee. Please compare against tracks whose key and BPM you know.
- **Tempo still often lands on half-time** for accent-alternating drum patterns (kick/snare backbeat). Fixing that properly needs a different onset representation and real reference tracks to validate; the prior change only helps part of it.
- Relative major/minor remains the most common key confusion.
- The Windows GitHub Actions build and FL Studio host testing have not been run. The sources were compiled and run here against JUCE 8.0.12 on Linux (GCC), including the editor.
