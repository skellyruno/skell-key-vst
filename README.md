# Skell Key V54 — Lean GitHub VST3 source

This is the **minimal build package** from V54. All original V54 `Source/` files are preserved byte-for-byte, including the neon-green 525 × 313 UI, key/chord/BPM/Reference A detectors, and priority drag-and-drop analysis.

## Build on GitHub

1. Put **the contents of this ZIP directly in the repository root**, not inside an additional folder. Make sure `.github/workflows/build.yml` is committed.
2. Open **Actions → Build Skell Key Windows VST3 → Run workflow**, or push to `main`.
3. After a successful build, download `Skell-Key-Windows-VST3` from the workflow's Artifacts section.

CMake downloads JUCE 8.0.12 during configuration. For a local JUCE checkout, configure with `-DJUCE_PATH=/path/to/JUCE` instead.

## Contents

- `Source/` — **all 22 unchanged V54 source and header files**, required by the plugin.
- `CMakeLists.txt` — JUCE 8 CMake project, VST3 format.
- `.github/workflows/build.yml` — Windows x64 GitHub Actions build and artifact packaging.
- `.gitignore` — keeps build output out of source control.

## Excluded deliberately

Historical benchmarks and logs, obsolete README/change files, old docs, standalone benchmark harnesses, Python training scripts, test files, generated artifacts, and the separate experimental model-training CI workflow. None is required to compile the VST3. Existing model-weight headers **are kept** because source headers depend on them, even though the learned model remains disabled until validated.

## Limitations

This cleanup does not modify DSP formulas or provide any new accuracy improvement. Test suites have been intentionally omitted from the minimal release; keep a copy of the full V54 archive if you want to run historical regression and research tests. Windows GitHub compilation and FL Studio host testing have not been performed in this environment.

## Windows MSVC fix (2026-10-24)

`Source/TonalPhraseTracker.h` now declares the fixed MIDI frequency-map bounds
inside its captureless static-initializer lambda. This corrects the MSVC
C3493/C2440 compile errors without changing the computed note frequencies or DSP.

The header was smoke-tested with GCC and Clang using C++17. The Windows
GitHub Actions VST3 build still needs to be rerun to verify MSVC/JUCE.
