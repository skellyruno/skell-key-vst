# TONIC Recreation V17 - UI Layout Fix

Updated from V16. This version fixes overlapping chord/checkbox controls, knob alignment,
footer file status clipping, progress-bar overlap, and corrupted special-symbol glyphs.

- Window size: 860 x 670 with separate key, tempo, automatic reference A, control,
  and footer sections.
- Reference A is detected automatically; there is no tuning knob.
- Piano keyboard remains removed.
- Existing DSP and drag-and-drop/file picker behavior are preserved.
- Long file status text is abbreviated, with full text available by tooltip.
- CMake and GitHub Actions are unchanged.

Replace your GitHub repository files with the archive contents and run Actions.
Compilation in Windows/FL Studio has not been verified in this environment.
