# Skell Key session history and recovered decisions

This is a curated engineering history from the user's November 12 Claude export and the November 20 handoff. It records durable implementation decisions and measurements, not every conversational turn or speculative feature brainstorm.

## 2026-10-25 to 2026-10-26 — V54 to V55

The initial task was to improve key/BPM accuracy and give the interface a skeleton-key identity.

The analyzer moved away from deciding the key from roughly the last half-second/current chord and toward long-term chroma evidence (approximately 20 seconds of accumulation). Spectral peaks were interpolated and assigned to nearest notes with a cosine taper, reducing neighboring-semitone leakage. Tuning estimation was expanded to approximately ±50 cents around A4 and the tuned frequency grid was used for note/chroma assignment. Krumhansl–Kessler and Temperley key profiles were blended.

File analysis was changed to preserve a result through trailing silence and to use whole-file evidence. A loop-length check was added for dotted-kick trap patterns that produced a 100 BPM autocorrelation peak for a 150 BPM track. The loop check was validated against 20 different trimmed versions of the Malta beat.

The UI became a Bone-on-black skeleton-key scene with a skeleton at a one-octave piano, a tonic-highlighted scale, input-level strip and tempo playhead, confidence teeth, A4 cents gauge, resizable layout and click-to-play behavior. The user later requested the larger 550 × 450 design space and then a 10% smaller compact window; the current V56 default is 455 × 373.

Historical V55 measurements reported in the session:
- Rendered real compositions: 54/150 exact key in V54 to 110/150 in V55; tempo within 3% 120/134 to 124/134; tuning mean absolute error 5.2 cents to 0.1 cents.
- Synthetic drum loops: key 69/288 to 223/288; exact tempo 10 to 34 of 263.
- These are historical stage results; the final V56 values are recorded separately in the accuracy document.

## 2026-10-26 — Malta reference case

The user corrected an early “no key found” / 100 BPM result. The plugin initially lost the readout at trailing silence and selected the dotted-kick 100 BPM alias. File analysis was changed to hold the result through silence, and the loop-length check made the 150 BPM grid stable across 20 trimmed cuts.

The accepted ground truth is B major, 150 BPM and A ≈ 450.8 Hz. This remains the primary regression anchor.

## 2026-10-27 — Maajins and V56 controls

The Maajins tempo was initially read at 80/78 BPM and later refined to approximately 156.1 BPM (the handoff records 156.06). The user changed their key guess during the conversation: A major, then E minor, then “might be D major.” The analyzer returned D major at very low confidence. The key remains uncertain; only the 156 BPM tempo is treated as ground truth.

V56 added /2 and x2 display correction buttons, a stricter alternative-tempo hint, refined tempo period estimation, playable piano and MIDI input, chord-loop playback, melody tracking, saved results, skin selection, and the explicit send-to-autotune control.

The original send control copied a text key and held the seven scale notes as MIDI output. This was later superseded by the SkellCore SEND TO TUNE protocol. Do not document the older MIDI/clipboard implementation as the current inter-plugin transport.

## 2026-10-28 to 2026-11-05 — feature decisions and verification requirements

The conversation explored tap tempo, key lock/override, runner-up display, melody-aware key, project-key memory, batch sample scanning, chord-chart export, key-change maps, and skeleton-band ideas. These were brainstorms, not all approved or implemented features. The current handoff confirms melody tracking, chord-loop preview and playable piano; it explicitly says the skeleton-band idea is not built.

The user repeatedly prioritized measured accuracy, a conservative confidence gate, an uncluttered UI and preserving existing host projects. The earlier session explicitly listed Windows VST3 build, DAW loading, live-input, state recall, CPU/thread and latency checks as separate release gates.

## 2026-11-06 to 2026-11-12 — GiantSteps+ accuracy work

Audiokeychain was rejected as a bulk benchmark source because the database did not provide audio/bulk access and was marked All Rights Reserved. GiantSteps+ was selected as the main external EDM benchmark, with Beatport EDM as a separate possible larger set.

The user downloaded labels/metadata and supplied multiple overlapping batches of MP3s. The unique usable GiantSteps+ audio set reached 67 tracks. The main failure group was parallel-mode confusion: minor-labelled tracks were often read as major with the same root when the major and minor third had similar energy.

A blanket minor prior was tested and rejected because it harmed the older real-composition set. An additive third-evidence term did not help. The narrower same-root parallel tie-break improved exact key matches from 41/67 to the final 49/67, reduced parallel errors from 10 to 2, improved both halves of an odd/even split (+5 and +3), and left the previously correct tracks unchanged in that reported run.

The final handoff recorded 48/60 tempo-labelled tracks matching. Several 3:2 discrepancies were judged likely label-convention errors after comparing genre and tempo; four cases remained genuine analyzer misses.

## 2026-11-11 to 2026-11-14 — SkellCore sync and DC robustness

Separate ChatGPT work integrated SkellCore's complete Key/Scale/A4 tuning snapshot and replaced the old MIDI/clipboard send action with explicit SEND TO TUNE. Windows uses a hub-free same-session shared-memory transport. Skell Key main commits included:
- 2ecaa10f — V56 source plus first SkellCore integration.
- 24ca8a25 — explicit send action and stable key/scale/A4 transfer.
- 9a988dc3 — hub-free SEND TO TUNE.
- 33187cd5 — canonical SkellCore dependency and corrected hub-free status.
- 4a154f2 — DC-offset-safe tonal analysis and regression tests.
- b66c5999 — later build workflow update for exact-source UI/release validation guidance.

The DC-only false-key issue was addressed by subtracting the mean from tonal-analysis FFT windows and using AC RMS for the key silence gate. This does not change the audible signal or tempo analysis and is not claimed as a GiantSteps accuracy gain.

## 2026-11-15 to 2026-11-20 — release and installer work

A series of stacked draft PRs added Windows VST3 binary/metadata verification, flat artifact staging, pluginval exit-code handling, strict moduleinfo normalization, checksum-backed install/update/rollback, safe installer-test cleanup, a valid default host program name, reparse-point checks, and public-repository withholding of installable preview artifacts.

The final stack head 60843d5 passed the Windows CI run 44695076204. The stack is consolidated in the repository audit document. A later PR attempting to restore the visible TAG control conflicts with the user's earlier “TAG hidden” decision and is excluded.

## Durable user decisions

- Keep Bone as the primary UI theme and keep the UI uncluttered.
- Do not reveal a result until confidence/stability gates are satisfied.
- Keep the BPM alternative hint strict and dismissible.
- Preserve Malta and all existing benchmark baselines.
- Keep TAG hidden.
- Keep skeleton-band as an unbuilt future idea, not a current feature.
- Do not infer a Maajins key from a low-confidence result.
- Do not treat brainstormed features as implemented.
