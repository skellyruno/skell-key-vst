# Skell Key accuracy baselines and roadmap

**Status date:** 2026-11-20  
**Source:** V56 handoff, November 12 Claude session export, current source comments and README.  
**Important:** scores below are historical reported measurements. The audio corpus and original /home/claude harness are not currently checked into this repository, so these numbers have not been rerun during the November 20 audit.

## Ground-truth regression anchors

| Material | Accepted reference | Required regression |
|---|---|---|
| Malta type beat | B major, 150 BPM, A ≈ 450.8 Hz | All 20 recorded trimmed cuts remain B major / 150 BPM |
| Maajins type beat | 156 BPM (about 156.06) | Tempo must remain near 156; key is uncertain and must not be forced |
| GiantSteps+ | 67 unique scored tracks with usable key labels | Preserve 49/67 exact as the current reported baseline until rerun |
| Older rendered real compositions | 150 tracks | Preserve 118/150 exact key |
| Synthetic sets a–f | 288 key tracks | Preserve 225/288 exact key |

## Reported accuracy progression

| Benchmark | V54 / early baseline | V55 | V56 / final handoff |
|---|---:|---:|---:|
| Rendered real-composition exact key | 54/150 (36%) | 110/150 (73%) | 118/150 (78.7%) |
| Rendered real-composition tempo within 3% | 120/134 | 124/134 | 126/134 |
| Rendered real-composition mean tuning error | 5.2 cents | 0.1 cents | 0.1 cents reported |
| Synthetic exact key | 69/288 | 223/288 | 225/288 |
| Synthetic exact tempo | 10/263 | 34/263 | 34/263 reported |
| GiantSteps+ exact key | not scored | 41/67 before tie-break | 49/67 after tie-break |
| GiantSteps+ tempo match | not scored | — | 48/60 labelled tracks |

The final GiantSteps+ key score is 49/67 (73.1%). An interim experiment briefly reported 50/67; the final fixed-constant tie-break and handoff settle on 49/67. Use 49/67 as the shipped baseline.

## GiantSteps+ corpus and scoring notes

- GiantSteps+ EDM Key Dataset: Zenodo record 1095691, 600 two-minute excerpts, audio and labels/metadata distributed separately, CC BY-SA 4.0.
- Beatport EDM Key Dataset: Zenodo record 1101082, a separate possible benchmark; it is not the source of the 67-track GiantSteps+ score.
- The user supplied keys.zip, original_metadata.zip and GiantSteps.xlsx, then sent approximately four overlapping batches of about 20 MP3s each. After de-duplication, 67 tracks with usable key labels were scored.
- Labels are about 85% minor. Some tracks list two keys or modal names, so an exact first-label score can overstate musical disagreement.
- Audiokeychain was not used: no audio/bulk access and the site was marked All Rights Reserved. Do not scrape it.
- The dataset license is CC BY-SA 4.0. Preserve the dataset's attribution and license terms if redistributing derived dataset materials.

### Final key error breakdown (67 tracks)

- Fifth-up errors: 6.
- Relative-key or mode-name disagreements: 6.
- Other: 4.
- Parallel same-root major/minor: 2 (down from 10 before the tie-break).

### Shipped parallel-mode tie-break

The rule runs after candidate ranking and before temporal key consensus. If the leader is major, compute the normalized minor-third/major-third contrast from long-term keyChroma:

d = (minorThird - majorThird) / (minorThird + majorThird + 0.01)

If absolute d is below 0.12 and the same-root minor candidate is within 0.20 score of the major leader, promote the same-root minor. Cross-root comparisons are untouched.

Reported result: 41/67 → 49/67 exact; parallel errors 10 → 2. Both halves of an odd/even ID split improved (+5 and +3), and the previously correct tracks remained correct in that run. The older real-composition set stayed 118/150, synthetic stayed 225/288, Malta stayed correct and Maajins tempo stayed approximately 156.06 BPM.

## Tempo cases still open

The final handoff says 48 of 60 GiantSteps+ tracks with BPM labels match. Six apparent 3:2 mismatches were considered likely label/convention errors after genre/tempo review; do not treat those as confirmed plugin errors without listening or checking beat subdivisions.

Four cases were identified as real analyzer misses:

| Track ID | Plugin result | Label/reference | Diagnosis to investigate |
|---|---:|---:|---|
| 305771 | 62.5 BPM | 125 BPM | Half-time |
| 5048263 | 55 BPM | 110 BPM | Half-time |
| 5035524 | 200 BPM | 100 BPM | Double-time |
| 1234670 | 155 BPM | 175 BPM | Non-octave disagreement |

Do not lower the half-time threshold globally. A looser sub-70 BPM threshold was tried and did not fix the target tracks. The V56 rule uses a separate half-time promotion check and an octave-jump hold bypass only when its evidence is strong.

## Accepted and rejected experiments

| Experiment | Outcome | Decision |
|---|---|---|
| Krumhansl–Kessler + Temperley blended profiles | Improved rendered/synthetic results | Shipped |
| Long-term key chroma (~20 s) | Major improvement over current-chord decision | Shipped |
| Tuned spectral peak assignment and ±50-cent tuning range | Improved tuning and semitone separation | Shipped |
| File opening/closing tonic evidence | Improved real-composition file key results; excessive weight flipped Malta to minor | Keep compromise weight |
| Parallel-mode tie-break | 41/67 → 49/67; no reported older-set regression | Shipped |
| Additive third-evidence term | Did not help | Rejected |
| Blanket minor prior | Older real-composition set fell 118 → 112 | Rejected |
| Out-of-scale / leading-tone penalty | Could reach about 127/150 but changed Malta B major to F# minor | Rejected |
| Melody most-frequent-note as key evidence | Helped Maajins read A major but reduced older set 118 → 111 | Rejected |
| Looser half-time threshold below 70 BPM | Did not fix the two slow misses | Rejected |

## Next accuracy pass

1. Reacquire at least 30 GiantSteps+ audio tracks with IDs not present in the original 67. The earlier 150-ID balanced list was named giantsteps_subset_ids.txt, but that file and the original /home/claude/harness were not found in this audit.
2. Score the original 67 and the unseen holdout separately. Do not tune on the holdout.
3. For fifth-up cases, inspect root-vs-dominant evidence, bass-root certainty, phrase endings, beat/downbeat evidence and the candidate score margin. Avoid global priors.
4. For relative/mode cases, distinguish true key errors from modal labels, relative-key equivalence and multi-key annotations.
5. For the four tempo cases, inspect onset/phase evidence and listen to the beat grid. Treat label convention separately from algorithm error.
6. After every DSP change rerun: GiantSteps original + holdout, 118/150 real compositions, 225/288 synthetic, Malta's 20 cuts and Maajins tempo.
7. Report exact key, parallel/relative/fifth error counts, tempo tolerance, confidence and any ambiguous/multi-key labels. Do not claim a gain from a correctness fix without corpus measurements.

## DC-only false-key correction

A pure DC file previously displayed A# major at low confidence. The shipped tonal-window correction removes the window mean before the FFT and uses AC RMS for the key silence gate. The dedicated test covers DC-only, a musical tone with a large DC offset, tiny AC and circular history offset. This is a false-key correctness fix; **no new GiantSteps accuracy score is claimed**.
