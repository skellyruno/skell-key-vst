# Skell Key documentation

This folder is the canonical engineering handoff for Skell Key. It consolidates the V56 handoff supplied in the November 20 conversation, the November 12 Claude session export, the current source tree, the SkellCore protocol pinned by CMake, and the November 15–20 release/installer PR stack.

## Read in this order

1. **[Project handoff](handoff/PROJECT_HANDOFF.md)** — current product state, preserved behavior, architecture summary, integration status and next actions.
2. **[Session history](handoff/SESSION_HISTORY.md)** — V54 → V55 → V56 milestones and decisions recovered from the earlier conversation export.
3. **[V56 technical details](handoff/V56_TECHNICAL_DETAILS.md)** — exact DSP settings, confidence/state fields, UI interactions and the historical test-harness layout.
4. **[Accuracy baselines and roadmap](accuracy/BASELINES_AND_ROADMAP.md)** — measured datasets, accepted/rejected experiments, unresolved key/BPM errors and the next holdout test.
5. **[Analysis and SkellCore sync](architecture/ANALYSIS_AND_SYNC.md)** — worker/audio-thread split, key/tempo/tuning pipeline, SEND TO TUNE packet and receiver requirements.
6. **[Validation matrix](testing/VALIDATION_MATRIX.md)** — what has passed, where it passed, and what is still unverified.
7. **[Repository audit](REPO_AUDIT_2026-11-20.md)** — PR-stack consolidation, the pluginval false-failure root cause, installer/release changes and excluded TAG-button change.

## Evidence rules

- Source and tests on the exact commit take precedence over an old conversation statement.
- CI results are tied to the exact commit and workflow run named in the validation matrix.
- Benchmark scores copied from the historical handoff are explicitly labelled historical until the audio corpus and harness are rerun.
- “Built” does not mean “tested in FL Studio”; “tested by pluginval” does not mean “accepted by a real DAW.”
- No accuracy gain is attributed to the DC-offset fix. It is a correctness fix for DC-only false keys.

## Source records used

- User-supplied “Skell Key – handoff summary” in the November 20, 2026 conversation.
- User-uploaded conversation export: Claude_Music key and BPM detection_2026-11-12T16-04.md (4,060 lines, 142,542 bytes; uploaded November 12, 2026). This repository stores a curated engineering history rather than the full conversational transcript.
- GitHub source and CI state inspected November 20, 2026, including main at b66c5999 and the verified release-stack head 60843d5.
- Repository AGENTS.md and shared SkellWorld development / Quick4 guidance.

Large audio datasets and the old standalone /home/claude/harness directory were not present in the current repository audit. Their absence is recorded in the accuracy and validation documents; no audio or benchmark output has been fabricated or silently reconstructed.
