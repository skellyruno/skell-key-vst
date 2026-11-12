# Skell Key V56 — SkellCore/SkellWorld Tuning Sync

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
- The Windows VST3 GitHub Actions build and key/scale/A4 protocol tests have passed for the current hub-free integration. A manual, two-plugin FL Studio host session is still required for release validation.

## V56 changes
- Tempo: half-time alias rule (2:1) plus sub-lag period refinement; 156 BPM trap loops now read ~156, not 78.
- Tempo card: /2 and x2 toggle buttons (green when on, one cancels the other) to correct an octave error by hand.
- The original MIDI-and-clipboard "Send to Skelltune" behavior has been superseded by the current **SEND TO TUNE** button described below; current builds do not inject a held scale or overwrite the clipboard when sending.
- Window default 495 x 405 (10 % smaller).
- Key (file mode): the opening/closing tonic-triad evidence is weighted 2.5x higher. Real-composition key accuracy 108 -> 118 / 150, synthetic 224 -> 225 / 288. Higher weights score better on pieces but flip the malta beat to minor, so 1.0 is the compromise.
- Tempo: the final period is refined to ~0.1 % so a 156 BPM loop reads 156.07, and trimmed cuts of the malta beat now all read 149.9-150.1.
- Tempo hint: when a credible octave alternative exists (x2 or /2), that button pulses a green outline. Click it to apply.
- Skeleton: nods on every beat at the detected tempo; right-click the scene to cycle costumes (none, headphones, top hat, shades).
- Tempo hint made stricter (alternative must be nearly as strong as the chosen tempo and seen ~20 analysis steps in a row) and is dismissed for good once you click /2 or x2 (until Reset, Live Input or a new file). Buttons no longer show a focus ring after a click.
- Tried and not shipped: an out-of-scale / leading-tone penalty for the key. It lifts real-piece accuracy 118 -> ~127/150 but flips the malta beat B major -> F# minor.
- Chord-loop playback: clicking the skeleton plays the track's recent chord progression (loop of 2-4 distinct chords, with 7ths, at the detected tempo). Falls back to the scale + tonic chord when fewer than two chords have been heard. Shift-click always plays the scale.
- Melody view: the most salient mid-register note per frame is tracked; dots on the piano keys show how often each pitch class led, the current note is lit, and the header lists the top three (e.g. "MELODY A E F#").
- Tried and not shipped: using the melody's most frequent note as extra key evidence. It would read the maajins track as A major but lowers real-piece key accuracy 118 -> 111/150.
- Confidence gate: nothing is shown until it is sure. Files show "Analyzing..." and reveal key, tempo, tuning and melody only once the whole file is done. Live input shows a key after it has stayed the same for 4 s, tempo and tuning after 3 s of steady readings.
- Default window 455 x 373 (8 % smaller than the previous 495 x 405).
- UI pass: larger secondary text, a "?" button (header) that lists every click action, brighter "click to play" hint, animated "..." instead of "--" while a file is analysed.
- Saved result: the last confirmed key, tempo and tuning are stored in the plugin state, so reopening a DAW project shows them straight away ("Saved result - play audio to refresh") until fresh audio gives a new confirmed result. Reset clears it.
- Skins: clicking the skull key (top-left) cycles Bone (the main look, default), Neon, Graveyard and Clean (light). The choice is saved with the project.
- Playable piano: click or drag the on-screen keys, play with the computer keyboard (A S D F G H J K = white keys, W E T Y U O L P = black keys) or a MIDI controller (the plugin now has a MIDI input). The played notes are added to the output after analysis, so they never disturb detection. "Lock" snaps every note to the detected scale.
- TAG (hidden for now; the code is still in PluginEditor.cpp): add the key and tempo to file names, e.g. loop.wav -> loop_Dmaj_156.wav. One file (with a confirmation), or every audio file in a folder (the plugin analyses them one by one, then asks before renaming). An older key/tempo stamp is replaced, name clashes are skipped, and "Undo last tagging" restores the old names.
- Layout pass: the footer is file name + Open Audio + Live Input. The top row of the piano scene now shows only the melody notes and the Lock button; the click hint appears only while hovering the scene.

- Robustness pass: NaN/inf samples are zeroed before analysis; a file that fails to decode now clears the previous file's key/tempo instead of leaving them on screen.
- Key: parallel-mode tie-break for bass-driven material (major vs same-root minor when the third is ambiguous). Scored on 67 GiantSteps+ EDM excerpts: 41 -> 49 exact; older real-composition and synthetic sets unchanged.

### Post-V56 accuracy hardening: DC-only files

- Tonal-analysis FFT windows now subtract the mean (DC component), and the key silence gate uses AC RMS instead of total RMS. Pure DC and DC-heavy noise floors no longer masquerade as pitched input in the key analyzer.
- No changes to the audio playback path, tempo analysis, MIDI routing, UI/Bone skin, or SkellCore key/scale/A4 sync.
- A new JUCE-free regression test checks constant DC, a musical tone with a large DC offset (same de-meaned spectrum), a near-DC signal, and the non-zero circular history write position. It runs in the Windows VST3 CI test step.
- This is a correctness fix, **not a demonstrated increase in GiantSteps accuracy**. Preserve the reported 49/67, 118/150 and 225/288 baselines pending fresh audio corpus testing. No scoring or tempo thresholds were adjusted.



## V56 SkellCore inter-plugin key, scale & A4 sync

**SEND TO TUNE** is the explicit sender trigger. The button toggles sharing on/off;
no key, scale, or A4 is published by Skell Key before clicking it. Once enabled, the sender
broadcasts the confirmed root, major/natural-minor mode, 12-note scale mask, and A4 reference
(400–480 Hz) over the authenticated localhost SkellCore WorldLink. It continues to send updates
when the confirmed key or tuning changes, and periodically refreshes them for late receivers.
An already-confirmed key is eligible to send immediately on click instead of waiting for a
second four-second confirmation; unconfirmed live guesses remain suppressed. A4=440 Hz is
used only if no reliable current or saved measurement exists.

**Customer installation (Windows):** Install both VST3 plugins normally,
open Skell Key and SkellyTune Mini (or OmniChoir) in your DAW, allow Skell Key
to confirm a key and click **SEND TO TUNE**. That's all: the shared SkellCore
code uses Windows same-session shared memory. **No Hub, installer, Python,
internet connection or extra receiver buttons are required.** When Skell Key
has no confirmed key the sender displays **WAITING FOR KEY** until detection
resumes; click SEND TO TUNE again to stop sharing. The connection stays enabled
through quiet passages and switching from file to live input.

The hubless packet carries the versioned `musical.tuning` snapshot and a
5-second freshness window to prevent crashed instances replaying old tuning.
The sender never writes to the clipboard or injects sustained MIDI scale notes.
Recipients retain their existing VST3/parameter IDs; Mini reflects received
Key, Scale and A4 in its saved/automatable controls. A manual piano-note mask
still overrides automatic scale selection until reset. If several Skell Key
instances broadcast, the most recent sender wins. SkellWorld Hub remains an
optional tool for the wider ecosystem and legacy informational topics, but it
is **not** required for this Windows tuning link. On non-Windows hosts, the
existing optional Hub transport remains the fallback.

For future autotune/choir plugins, link the pinned `SkellCore::UI` CMake target, create
`skellcore::world::WorldLink` on a non-audio-owned processor and, after user opt-in,
read `latestMusicalTuning(skellcore::world::MusicalTuning&, uint32_t&)` on a JUCE timer. Copy
snapshot values into lock-free state for the DSP callback. Only accept a valid trusted `skell.key`
peer. The `musical.key` legacy topic carries root/mode only; the `musical.tuning` message also
carries an absolute pitch-class mask and A4 as integer millihertz.

**Offline validation:** Set `SKELL_KEY_BUILD_SYNC_TESTS=ON` when configuring and run CTest.
Builds pin SkellCore to immutable commit `568a17fd1956065f06d20813f5e57c3621c1cac9`.
