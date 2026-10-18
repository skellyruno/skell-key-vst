# TONIC-inspired V18 — Compact UI redesign

Real implementation of the new dark-blue SKELL key-analyzer interface in JUCE C++.

- Editor: **650 x 485 px** (about **45.4% less window area** than 860 x 670 V17).
- Vector-drawn cards, restrained blue highlights, waveform accents and revised typography.
- Removed Unicode punctuation from UI strings to avoid the broken-character issue seen in Windows screenshots.
- Existing real-time key, chord, BPM, auto Reference A, file-drop and DAW audio functions retained.
- No piano keyboard; Reference A is a detector, not a knob.
- Two adjustable knobs remain: Silence Gate and Responsiveness.
- GitHub Actions workflow preserved.

## GitHub installation
Replace your repository's files with the contents of this archive, commit to `main`, and run Actions > Build TONIC-inspired VST3.

## Validation
Code packaging and source checks done. A Windows JUCE compile and in-host visual test are still required. The design image is a visual reference, not an image stretched over the interface.
