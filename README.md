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
- **Files keep their result through silence.** A track ending in silence or a fade used to blank the key and BPM to "No key found"; file analysis now holds the result and uses the whole-file average for the key.
- **3:2 tempo alias (loop-length check):** dotted-kick (trap) patterns autocorrelate at 2/3 of the real tempo (100 instead of 150). The song's dominant long-range loop (e.g. 4 bars) must be a whole number of beats at the true tempo, so when the loop fits the faster grid but not the slower one, the faster tempo is chosen and held. On a real trap track this gave 150 BPM for all 20 trimmed versions of the file; the previous phase-strength rule only did so by luck.
- **Tempo prior strengthened** (log-normal around 120 BPM, weight 0.40, was 0.08) to reduce half-time answers.

**UI** — skull-bowed skeleton-key mark, bone-on-black palette with one ecto-green accent. A skeleton plays a one-octave piano: keys in the detected key are bright, the tonic is green, and every key glows with the live energy of its pitch class. **Click the skeleton to hear the detected key** (the scale ascending, then the tonic triad); the hands and keys follow the notes. The strip under the header is an input level meter with a playhead that sweeps one bar at the detected BPM. Key-bit "teeth" show confidence, a cents gauge shows Reference A, and the default window is 550 × 450 and resizable (fixed aspect). The `preferFlats` parameter is honoured in the display.

The preview tone is mixed into the plugin output only while it plays, after analysis has taken its copy of the input, so it never affects detection. Nothing is added to the output otherwise.
- **Opening and closing chords** (file analysis only): how a piece starts and ends is matched against each key's tonic triad as extra evidence.
- **Key profiles** are now the average of Krumhansl-Kessler and Temperley.

## Measured results (headless, rendered audio with known answers)

**Real compositions** — 150 tracks rendered from public Humdrum scores (Bach chorales, Mozart and Beethoven sonatas), transposed into all keys, 0 to 35 cents detuned, first 80 s:

| | V54 | V55 |
|---|---|---|
| Key exact | 54 / 150 (36 %) | 110 / 150 (73 %) |
| Tuning, mean abs error | 5.2 cents | 0.1 cents |
| Tempo within 3 % | 120 / 134 | 124 / 134 |

**Synthetic loops with drums** (288 key tracks, 263 tempo tracks): key 69 → 223 correct; tuning error ~12 → 0.2 cents; exact tempo 10 → 34.

Key results are stable across 22.05 / 44.1 / 48 / 96 kHz; V54 flipped keys between rates. A real trap track (B major, 150 BPM, A = 450 Hz) is read correctly in all 20 trimmed versions.

## Limitations

- The test audio is **rendered from scores or generated**, not commercial recordings, so treat the numbers as evidence of direction, not a guarantee. Compare against tracks whose key and BPM you know.
- **Tempo still often lands on half-time** for accent-alternating drum patterns (kick/snare backbeat). Fixing that properly needs a different onset representation and real reference tracks to validate; the prior change only helps part of it.
- Relative major/minor remains the most common key confusion.
- The Windows GitHub Actions build and FL Studio host testing have not been run. The sources were compiled and run here against JUCE 8.0.12 on Linux (GCC), including the editor.

## V56 changes
- Tempo: half-time alias rule (2:1) plus sub-lag period refinement; 156 BPM trap loops now read ~156, not 78.
- Tempo card: /2 and x2 toggle buttons (green when on, one cancels the other) to correct an octave error by hand.
- "Send to Skelltune" replaces Pause: holds the detected scale as MIDI notes (C4-B4 octave) on the MIDI output and copies the key to the clipboard. Route the plugin's MIDI output to your autotune's MIDI scale input (host dependent).
- Window default 495 x 405 (10 % smaller).
- Key (file mode): the opening/closing tonic-triad evidence is weighted 2.5x higher. Real-composition key accuracy 108 -> 118 / 150, synthetic 224 -> 225 / 288. Higher weights score better on pieces but flip the malta beat to minor, so 1.0 is the compromise.
- Tempo: the final period is refined to ~0.1 % so a 156 BPM loop reads 156.07, and trimmed cuts of the malta beat now all read 149.9-150.1.
