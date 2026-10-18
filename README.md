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
