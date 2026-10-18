# TONIC-inspired VST3 starter

This is an independent clean-room starter based on observable plugin metadata, embedded strings,
and the supplied screenshot. It does not contain copied TONIC code or assets.

See `docs/ANALYSIS.md` for binary findings, limitations, and build instructions.
Requires JUCE and CMake. No compiled plugin is included.

## Version 2 changes
- Replaced per-note time-domain sinusoid scans with a fixed-size 16384-point FFT.
- Added exponential chroma smoothing and three-frame candidate hysteresis.
- Reused preallocated analysis buffers and exposed the APVTS safely to editor attachments.
- Reset key state when playback preparation restarts.

**Known limitations:** This FFT is still executed synchronously on the audio callback; for production, use a worker with bounded queues and explicit timing tests. The key estimator remains heuristic and is not a replica of the original TONIC algorithm. No VST3 binary has been built or host-tested in this environment.

V3: worker-thread analysis and audio callback isolation; see docs/V3_CHANGES.md.

## V4 GitHub build fixes
- JUCE is downloaded automatically using CMake FetchContent (8.0.12), unless JUCE_PATH is supplied.
- Generated `JuceHeader.h` is enabled via `juce_generate_juce_header`.
- Replaced invalid `Graphics::drawArc` usage with a `juce::Path` stroke.
- Added missing JUCE plugin factory (`createPluginFilter`).
- Included `.github/workflows/build.yml` for Windows GitHub Actions and downloadable VST3 artifact.

**Not compiled or tested here.** If the GitHub workflow reports new compiler errors, provide the log.
