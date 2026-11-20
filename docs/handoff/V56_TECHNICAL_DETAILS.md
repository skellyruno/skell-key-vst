# V56 technical handoff details

This file preserves implementation-level details from the user's V56 handoff that are useful when resuming DSP, UI or host work. The source tree remains authoritative if a detail is later changed.

## Source package and environment

The November 12 handoff identified the source archive as /home/claude/Skell_Key_V56.zip and its extracted source as /home/claude/skell/. The original local harness lived under /home/claude/harness. Those paths are historical environment paths and are not guaranteed to persist or exist in this repository checkout.

Project: JUCE 8.0.12, Windows x64 VST3. Internal CMake target is TonicRecreation; displayed product name is Skell Key. The main visual design is Bone.

## Analysis pipeline

### Audio ownership

- Audio thread feeds a FIFO and renders preview/synth/MIDI output.
- Analysis worker consumes the FIFO and owns FFT/key/tempo analysis.
- The audio callback does not perform file I/O, blocking analysis or shared-memory IPC.
- The handoff describes the FIFO and published processor values as lock-free.

### Key evidence

- FFT size 16,384; hop 4,096.
- Chroma is formed from spectral peak picking with tuning compensation.
- Key profiles blend Krumhansl–Kessler and Temperley.
- SHS note salience and long-term keyChroma are used alongside short-term chroma.
- Other evidence includes bass, phrase, beat, downbeat, relative-mode and temporal-consensus bonuses.
- File analysis accumulates the first 10 seconds into startChroma and tracks endChroma with an approximately 6-second memory that holds through a silent tail.
- Current source computes boundary evidence as 0.5 times the average of start/end tonic-triad cosine matches: 1.0 * 0.5 * (startTriadCos + endTriadCos). Live analysis receives no boundary bonus. The README's “weight 1.0” is the multiplier; the effective average coefficient is 0.5.
- The parallel-mode tie-break is applied after ranking and before temporalKeyConsensus.propose. Its exact thresholds and benchmark result are in BASELINES_AND_ROADMAP.md.

### Tempo evidence

- Novelty envelope is 100 Hz.
- Normalized autocorrelation searches 40–240 BPM.
- Perceptual prior is log-normal around 120 BPM, sigma 0.75 octave, weight 0.40.
- The 3:2 phase checks and loop-length rule address dotted-kick patterns such as 100-versus-150 BPM.
- The 2:1 half-time promotion compares fast.first >= slow.first * 1.35. When halfTimePromoted is set, it bypasses the octave-jump hold.
- Final period refinement checks ±16 steps at 0.1% increments.
- The alternate tempo hint uses altTempoRatio and a 20-step persistence streak. It is strict and dismissible.
- /2 and x2 are mutually exclusive display corrections; both reset on Reset, Live Input or a new file.

### Tuning and robustness

- Reference A tuning search spans approximately ±50 cents.
- Tuning histogram selects the stable deviation and the tuned grid feeds note/chroma extraction.
- analyseAudioSample sanitizes non-finite samples to zero.
- Failed file decode clears the live queue and analysis state so an earlier key is not left visible.
- DC-only correction subtracts the mean from the tonal FFT window and uses AC RMS for the key silence gate. It does not change audible input/output or tempo.

## Editor and user interaction details

- The editor is painted in a 550 × 450 design space and scales to a 455 × 373 default window.
- SceneHitButton overlays the skeleton scene.
- The confidence gate is keyHoldMs = 4000 ms and bpmHoldMs = 3000 ms for live results. File mode waits for the full file analysis before revealing the final key/tempo/tuning/melody.
- Saved results persist through the APVTS state tree using savedRoot, savedMode, savedBpm, savedHz and savedTuneConf; theme selection is also saved.
- Clicking the key logo component with ID keymark cycles Bone, Neon, Graveyard and Clean. Bone remains the default/main look.
- The question-mark help overlay lists the click actions.
- /2 and x2 are mutually exclusive and turn green when active.
- The skeleton scene can play the recent chord progression; Shift-click forces the scale plus tonic triad.
- The chord-loop preview uses a 64-voice array, 16-entry chord-history ring and a 2–4 chord period finder.
- Melody tracking reports the most salient mid-register note in MIDI 55–84.
- The playable piano has 16 voices, supports mouse/drag, computer keyboard and MIDI controller input, and scale-lock snapping.
- The preview synth is mixed after the analysis copy and therefore cannot contaminate key detection.

## TAG behavior

The file-tagging implementation remains in source: it can stamp key/tempo into a filename, batch process a folder, replace an older key/tempo stamp, skip name collisions and undo the last tagging operation. The visible TAG control is intentionally hidden. Do not restore its visibility without an explicit user request.

## Historical local test harness

The original harness was outside the repository at /home/claude/harness and may not persist. The handoff records:

- JUCE modules precompiled under mods/*.o; flags.sh provides JFLAGS and JLIBS.
- build_hc.sh <Source> <out> builds the headless analysis harness; build_san.sh <Source> <out> <sanitizers> <harness.cpp> builds sanitizer variants.
- harness3 through harness16 covered snapshots, chord loop, gating, saved result, themes, live notes, tagging, timing, sanitizers, edge cases and keyChroma dumps.
- pscore.py <exe> data/k scored the older 150-track real-composition set.
- evalall.sh <exe> scored synthetic sets a–f.
- gsscore.py <exe> <audio dir> scored GiantSteps against the spreadsheet and metadata JSON.
- data/mp3cuts held the 20 Malta cuts.
- Source_gs was an experimental source copy with environment-variable knobs and was not shipped.
- Harness mode encoding: 0 = major, 1 = minor.
- The 150-ID holdout list was named giantsteps_subset_ids.txt.

None of those scripts/data folders is currently confirmed in this GitHub tree. Rebuild or reacquire them before reporting new benchmark scores; do not assume the old temporary directories survived.

## Host compatibility flags

CMake still has NEEDS_MIDI_INPUT=TRUE and NEEDS_MIDI_OUTPUT=TRUE. MIDI input is used for the playable piano. The current SEND TO TUNE button uses SkellCore shared-memory publication rather than the old held-scale MIDI notes. Re-check these flags in FL Studio and Ableton before changing them; host behavior has not been accepted in a real DAW session.
