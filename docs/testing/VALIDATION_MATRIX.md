# Skell Key validation matrix

## Exact evidence available as of 2026-11-20

| Area | Evidence | Status / limitation |
|---|---|---|
| V56 key and tempo accuracy | Historical rendered-composition, synthetic and GiantSteps+ scores in BASELINES_AND_ROADMAP.md | Not rerun in this audit; source audio and old harness unavailable |
| Parallel-mode fix | 49/67 GiantSteps+ exact, parallel errors 10 → 2; older sets and Malta unchanged in reported run | Historical corpus result; needs unseen holdout |
| DC-only false key | JUCE-free tonal-window regression test; GCC/Clang and ASan/UBSan reported passing; PR #5 merged | Correctness fix only; no GiantSteps gain claimed |
| SkellCore packet | 144 key/mode/A4 round trips plus malformed-frame tests in test_world_tuning.cpp | Portable protocol test; not a real two-plugin host session |
| Tonal window | test_tonal_window.cpp: DC-only, DC-biased tone, tiny AC, circular write offset | Included in Windows CTest |
| Windows VST3 build | Release-stack PR #15 head 60843d5, workflow run 44695076204 | Passed MSVC Windows x64 build |
| CTest | PR #15 run: 3/3 passed | Exact head 60843d5 |
| Python regressions | PR #15 run: 18/18 passed | Metadata normalizer, private artifact policy and CMake FetchContent contracts |
| Installer/update/rollback | PR #15 run: 33 PowerShell assertions passed | CI temp-root tests, not a real user installation |
| pluginval | pluginval 1.0.4, strictness 5, final SUCCESS | Separate Steinberg validator was skipped because its executable path was not configured |
| VST3 metadata | Strict moduleinfo normalization, processor/controller class IDs, PE32+ x64 binary and checksums | Validates staged bundle metadata, not DAW acceptance |
| Public artifact policy | PR #15 run emitted withholding notice; installable artifact upload was skipped | Only a diagnostic pluginval report was uploaded |
| Current-source Quick4 UI | Repo AGENTS.md says no verified native Quick4 renderer exists yet | Unverified; no screenshot should be described as current |
| FL Studio / Ableton | No accepted manual two-plugin session in the handoff | Unverified |
| CPU / latency in DAW | Local harness only | Real host, target PC and live CPU/latency unverified |
| Licensing / signing | Product license, third-party review, vendor URL/support and signing not approved | Commercial release blocked |

## Robustness results from the V56 handoff

The earlier local Linux pass reported:
- Mean audio block time under 8 microseconds across 44.1–192 kHz and 32–2048 sample blocks.
- Worst measured spike 343 microseconds at 48 kHz with 128-sample blocks (12.8% of the block budget).
- ASan/UBSan clean for live input, MIDI, scale lock, file analysis, re-requests during analysis, state round-trip, resizing and sample-rate cycling.
- TSan reported 20 warnings around thread start/stop and FIFO handoff where JUCE synchronization was not visible to the instrumented build; no steady-state warning was reported. These were treated as instrumentation false positives, not proven universally harmless.
- Edge inputs covered mono, six-channel, anti-phase, clipped, DC, NaN/Inf, 24-bit, FLAC/OGG/MP3, 8 kHz/11 kHz/192 kHz, 50 ms and one-sample files, empty/zero-byte/random/text/truncated/bad-header files and a 13-minute file. No crash or non-finite output was reported; the 12-minute analysis cap held.
- analyseAudioSample zeroes non-finite samples. Failed decode calls discardLiveQueue() and resetAnalysis() so an old key is not left visible.

These are handoff-reported local results, not a fresh run on the November 20 Windows release candidate.

## Release-stack Windows run

PR #15 exact head: 60843d5ea3f64c85b378d98cf94a039a0a13b63f  
Workflow: [run 44695076204](https://github.com/skellyruno/skell-key-vst/actions/runs/44695076204)

The recorded run passed:
- CMake configure and MSVC 19.51.36260.0 Release VST3 build.
- CTest 3/3.
- Python regression tests 18/18.
- Strict VST3 moduleinfo validation and one-level bundle staging.
- PowerShell private-preview install/update/rollback tests: 33 assertions.
- pluginval 1.0.4 strictness 5 with final SUCCESS.
- Public-repository withholding notice; installable product ZIP upload was skipped.

The recorded Windows module SHA-256 was e58dac3d456cd895f8d2cf369d8cc41cf31a0a72fdf2d74fda6eb02342857b7a. The diagnostic-only pluginval artifact contained logs/report, not the VST3. This run is release-stack CI evidence, not a stable release or a real host test.

## Still required before beta/release

1. Obtain and test the installable artifact through an owner-approved private testing channel.
2. Install/rescan/load in FL Studio and Ableton; verify audio input, file analysis, live analysis, state save/reopen and clean close.
3. Test Skell Key → SkellyTune Mini in one real FL Studio project; confirm key, scale and A4 update correctly and remain stable through silence/file-live transitions.
4. Verify OmniChoir/AdlibLab A4 is consumed by their DSP rather than merely received.
5. Check MIDI input/output flags and host routing behavior.
6. Run CPU, latency, glitch, clipping, denormal and real-time-safety checks in the actual host at target sample rates/buffer sizes.
7. Implement the real JUCE Quick4 renderer described by AGENTS.md, compile it from current source and inspect its four actual screenshots. Do not use mockups or old CI screenshots.
8. Re-run the original accuracy sets and a 30+ track unseen GiantSteps+ holdout.
9. Complete JUCE/SkellCore licensing and attribution review, product license approval, vendor URL/support details and signing before commercial distribution.
