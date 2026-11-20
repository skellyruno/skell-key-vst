# Skell Key analysis architecture and SkellCore tuning sync

## Audio and worker-thread separation

The audio callback feeds the analysis FIFO and handles output-side preview/synth/MIDI work. The analysis worker owns the FFT, chroma, key and tempo analysis. The callback must remain real-time safe: no allocations, locks, file access, network access or blocking analysis in processBlock.

Recorded V56 analysis settings:
- FFT size: 16,384.
- Hop: 4,096 samples.
- Tempo novelty envelope: 100 Hz.
- Tempo search: normalized autocorrelation over 40–240 BPM.

The exact current implementation in Source/PluginProcessor.cpp is authoritative if these settings change.

## Key detection

The key estimator combines:
- Krumhansl–Kessler and Temperley key profiles.
- Spectral peak note salience and tuning-histogram compensation.
- Short-term chroma for chord/UI behavior and long-term keyChroma for key ranking.
- SHS note salience and tonal harmonic evidence.
- Bass, phrase, beat, downbeat and relative-mode evidence.
- Temporal key consensus.
- File-mode opening/closing tonic-triad evidence (not applied to live mode). Current source computes 0.5 times the average of the opening/closing tonic-triad cosine matches; live analysis receives no boundary bonus.
- The narrow same-root parallel-mode tie-break described in the accuracy document.

The final parallel tie-break is deliberately not a blanket minor prior. It only acts when the major/minor third evidence is ambiguous and the same-root minor candidate is already close to the leader.

## Tempo detection

The tempo path uses a 100 Hz novelty envelope and normalized autocorrelation over 40–240 BPM. The V56 logic includes:
- A log-normal perceptual prior centered around 120 BPM (sigma 0.75 octave, weight 0.40).
- 3:2 phase checks and a loop-length rule for dotted-kick patterns such as the Malta 100-vs-150 alias.
- A separate 2:1 half-time promotion rule. The handoff records the comparison fast.first >= slow.first * 1.35 and the halfTimePromoted flag, which bypasses the octave-jump hold only when promoted.
- Fine period refinement over ±16 steps at 0.1% increments.
- A strict alternate-tempo hint: the alternative must persist for about 20 analysis steps. The hint is dismissible and should not become eager.
- /2 and x2 controls are mutually exclusive manual display corrections.

Do not change a global prior or tempo threshold based on one track.

## Tuning and DC handling

Tuning detection covers approximately ±50 cents around A4. The detected tuning shifts the note grid used for chroma, so off-standard sampled material can be analyzed without treating its detuning as a different key.

The DC-only false-key correction is isolated to the tonal analysis window:
- Subtract the window mean before applying the Hann window and FFT.
- Use AC RMS for the key silence gate.
- Do not modify the audible input/output path or tempo detector.
- The dedicated JUCE-free test verifies constant DC is rejected and a tone with a large DC offset produces the same de-meaned spectrum as the clean tone.

## Chord, melody and playable piano

- Chord-loop preview uses a 64-voice array, a 16-entry chord history ring and a 2–4 chord period finder.
- The melody tracker reports the most salient note in MIDI range 55–84.
- The playable piano has 16 voices, supports computer-keyboard and MIDI-controller input, and can scale-lock played notes to the detected scale.
- Preview audio is rendered after the analysis copy, so the plugin's own preview does not contaminate key detection.
- File tagging/renaming exists in code, including undo, but the TAG button is intentionally hidden.

## SkellCore protocol and transport

The SkellCore dependency is pinned in CMake to commit 4c499c58cb3b2791eed2d5dcef6d8d543fcaea8d from skellyruno/skellcore-vst-template. The Windows LocalTuningBus default mapping name is Local\\SkellCore.MusicalTuning.v1. The pin includes:
- WorldTuning.h: validated MusicalTuning snapshot and wire format.
- LocalTuningBus.h: Windows same-session shared-memory transport.
- WorldLink.h: optional Hub transport for legacy/informational SW1 topics and a wrapper around local tuning publication/reading.

### Packet contents

One complete snapshot contains:
- Root pitch class, 0–11.
- Mode: 0 major, 1 natural minor.
- Absolute 12-note pitch-class mask.
- A4 reference in integer millihertz, valid from 400,000 to 480,000 (400–480 Hz).

The 64-bit shared word carries the snapshot plus a 28-bit monotonic timestamp in 100 ms ticks. A packet expires after 50 ticks (5 seconds). Normal shutdown clears only the sender's own last packet; a newer publisher's packet is not erased. With multiple Skell Key instances, the most recent publisher wins.

### Sender behavior

- SEND TO TUNE is explicit opt-in. No musical tuning is published before the user enables it.
- The sender requires a confirmed key; it can publish an already-confirmed result immediately after the click.
- It updates when confirmed key or tuning changes and periodically refreshes for late receivers.
- The sender remains enabled through quiet passages and switching between file/live modes.
- If no confirmed key is available, the UI reports WAITING FOR KEY. Clicking again stops sharing.
- A4 defaults to 440 Hz only if there is no reliable current or saved measurement.
- The current SEND TO TUNE button does not use the earlier held-MIDI-scale or clipboard behavior.

### Receiver behavior and limits

- Receivers open/join and poll on a JUCE message/worker timer, never from the audio callback.
- Copy the latest snapshot into lock-free processor state before DSP consumes it.
- SkellyTune Mini reflects received Key, Scale and A4 in its saved/automatable controls; a manual piano-note mask still overrides automatic scale selection until reset.
- OmniChoir and AdlibLab can receive tuning snapshots, but A4 being present in the packet does not prove their DSP uses it. Full A4 tuning consumption remains unfinished/unverified in those engines.
- The full SkellyTune pairing and actual FL Studio two-plugin session have not been accepted in a real host.

### Transport security and platform behavior

On Windows, LocalTuningBus uses a named mapping in the Local namespace, scoped to the interactive logon session by default Windows object security. It is a same-user informational bus, **not an authentication boundary**. Another process running as the same user can impersonate a Skell Key sender. Do not describe this path as authenticated or use it for privileged commands.

The optional localhost SkellWorld Hub remains available for legacy informational topics and non-Windows fallback. The Windows Key/Scale/A4 path does not require the Hub, a service, Python, internet access or an installer.

## Host MIDI flags

CMake still declares NEEDS_MIDI_INPUT=TRUE and NEEDS_MIDI_OUTPUT=TRUE. MIDI input supports playing the piano from a controller. The current SEND TO TUNE action uses SkellCore rather than sustained MIDI scale notes. Whether MIDI output is still required, and how each host interprets the flags, remains a separate FL Studio/Ableton compatibility check. Do not remove or change the flags until tested in the actual hosts.
