# Skell Key — canonical project handoff

**Last consolidated:** 2026-11-20  
**Repository:** skellyruno/skell-key-vst  
**Product:** Skell Key, JUCE 8.0.12, Windows x64 VST3  
**Current product generation:** V56-era analyzer and Bone-first skeleton-key UI

## Current state

Skell Key analyzes musical key, chord, BPM and reference A tuning from live input or a dropped/opened audio file. The V56 source includes the long-term key estimator, tuned spectral peak evidence, key/chord/melody views, tempo alias handling, confidence gating, file analysis, playable piano and the explicit SkellCore **SEND TO TUNE** connection.

The November 20 repository audit found:

- Main before release-stack consolidation: b66c5999ff9ec2c5fb1d72f35ae8e57319805fad.
- The November 15–20 release/installer work is a verified stacked candidate ending at 60843d5ea3f64c85b378d98cf94a039a0a13b63f. It contains PRs #7–#15; see the repository audit and validation matrix.
- The DC-offset-safe tonal-window fix is already in main at 4a154f2984d7.
- The accuracy roadmap remains tracked in GitHub issue #6.
- PR #16's visible TAG-button restoration is intentionally excluded. TAG stays hidden unless the user explicitly changes that decision.

The release/installer stack changes build, metadata validation, packaging and install/rollback tooling; it does not intentionally change DSP, the Bone UI, VST3 class IDs, published parameter IDs, presets, automation IDs or serialized host state.

## Product behavior to preserve

- Bone is the default and main skin. Other shipped skins are Bone, Neon, Graveyard and Clean.
- Keep the interface uncluttered and the confidence gate conservative. Do not expose a key before it is stable.
- The default editor is the compact V56 design (455 × 373); the original 550 × 450 design-space layout is scaled responsively.
- The skeleton/piano scene, chord-loop preview, melody indicators, playable piano, scale lock, /2 and x2 controls, strict dismissible tempo hint and saved-result behavior are part of the current design.
- The TAG file-renaming feature exists in source but its visible control is hidden by explicit product decision.
- Preserve all existing VST3 identity, parameter/automation IDs, presets, saved plugin state and FL Studio projects.
- Do not change key or tempo scoring thresholds without rerunning the full regression corpus.

## Analyzer architecture at a glance

- JUCE audio callback feeds a FIFO; the analysis worker owns FFT and key/tempo analysis.
- The audio callback remains responsible for preview/synth/MIDI handling and does not perform file I/O or blocking analysis.
- FFT size is 16,384 samples with a 4,096-sample hop.
- Key evidence blends Krumhansl–Kessler and Temperley profiles with long-term chroma, spectral peak salience, tuning compensation, bass/phrase/beat/downbeat evidence, relative-mode evidence and temporal consensus.
- File analysis includes start/end tonic-triad evidence. Live analysis does not use the file-boundary bonus.
- Tempo uses a 100 Hz novelty envelope, normalized autocorrelation, loop/phase checks, octave handling, temporal continuity and fine period refinement.
- Tuning estimates cover ±50 cents around A4 and the tuned note grid is used for chroma.
- See [Analysis and SkellCore sync](../architecture/ANALYSIS_AND_SYNC.md) for implementation details.

## Accuracy ground truths and guardrails

These are non-negotiable regression anchors:

- **Malta type beat:** B major, 150 BPM, A ≈ 450.8 Hz. All 20 trimmed cuts in the recorded handoff read B major / 150 BPM.
- **Maajins type beat:** 156 BPM (the handoff records approximately 156.06 BPM). Its key is not settled: the user considered A major, E minor and D major; the analyzer's D-major result had about 9% confidence in the latest handoff (an earlier session recorded an even lower value). Do not turn D major into ground truth.
- The final reported GiantSteps+ result is 49/67 exact keys after the parallel-mode tie-break. The earlier 50/67 was an interim experiment, not the final fixed-constant result.
- The older real-composition set is 118/150; synthetic sets a–f are 225/288. These are historical results and must be rerun before claiming a new accuracy gain.

Full scores and unresolved cases are in [Accuracy baselines and roadmap](../accuracy/BASELINES_AND_ROADMAP.md).

## SkellCore integration

**SEND TO TUNE** is explicit opt-in from Skell Key. It sends a complete musical tuning snapshot: root, major/natural-minor mode, absolute 12-note pitch-class mask and A4 reference in millihertz.

On Windows, the transport is SkellCore's same-session shared-memory LocalTuningBus. No SkellWorld Hub, background service, Python process, internet connection or receiver button is required. The receiver opens/polls the bus off the audio thread. A five-second freshness window expires abandoned packets; the most recent sender wins if several Skell Key instances are active.

Important security qualification: LocalTuningBus is same-user informational IPC, **not an authentication boundary**. Another process running as the same Windows user could impersonate a sender. Do not describe the Windows shared-memory path as authenticated. The optional Hub path has peer/product registration checks for its event transport.

The CMake dependency is pinned to SkellCore commit 4c499c58cb3b2791eed2d5dcef6d8d543fcaea8d. The local override SKELLCORE_PATH remains available. See the architecture document for receiver guidance and remaining A4 integration work.

## Current build and release shape

- Windows x64 VST3 only; no standalone target.
- JUCE 8.0.12.
- Product version in the staged release candidate: 0.1.0, private preview only.
- CMake uses the pinned SkellCore source and offers SKELL_KEY_BUILD_SYNC_TESTS=ON.
- The public GitHub repository deliberately withholds the installable preview ZIP. CI can build and validate the product, but the artifact upload is skipped on public runs. Do not publish a public installable artifact to work around this boundary.
- The PowerShell installer is optional, per-user, and designed for install/update/rollback with checksums and backups. It is not a signed commercial installer.

## Next priorities

1. Reacquire the GiantSteps audio batches and the old scoring harness (or rebuild a reproducible harness from the source). The prior /tmp/gsall and /home/claude/harness paths may no longer exist.
2. Obtain at least 30 unseen GiantSteps IDs and run a held-out evaluation; keep the original 67-track set as a tuning set.
3. Investigate fifth-up and relative/mode errors separately. Do not use a blanket minor prior.
4. Diagnose the four real tempo misses separately; do not lower the half-time threshold globally.
5. Rerun Malta (20 cuts), Maajins tempo, the 118/150 real-composition set and 225/288 synthetic sets after every scoring change.
6. Verify the exact Windows VST3 in FL Studio and Ableton, including SEND TO TUNE with SkellyTune Mini, project save/reopen, MIDI host behavior, CPU and latency.
7. Implement and validate Skell Key's real JUCE Quick4 renderer before claiming a current-source UI screenshot. Repo AGENTS.md explicitly says the renderer is not yet verified.
8. Finish A4 DSP consumption in OmniChoir and AdlibLab; receiver packet reception alone does not prove the audio engine uses A4.

## Build / test entry points

- Configure Windows x64: cmake -S . -B build -A x64 -DSKELL_KEY_BUILD_SYNC_TESTS=ON
- Build: cmake --build build --config Release --parallel 2
- Test: ctest --test-dir build -C Release --output-on-failure
- The Windows workflow also runs Python regression tests, metadata normalization, private-preview installer/update/rollback tests and pluginval strictness 5.

See [Validation matrix](../testing/VALIDATION_MATRIX.md) for exact commit/run evidence and unverified gates.
