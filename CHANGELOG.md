# Changelog

## Unreleased
- Restore the existing TAG file-stamping action to the visible editor control lifecycle; apply compact-button styling and keep it synchronized with help, file-drag overlays, theme repaint and LookAndFeel teardown. No parameter IDs, DSP, presets or saved state changed.
- Replace deprecated `FetchContent_Populate` usage with pinned `FetchContent_MakeAvailable(SkellCoreRepo)` and `SOURCE_SUBDIR SkellCore`; preserve the immutable SkellCore revision and remove the CMake 4.4 deprecation warning.
- Withhold the installable Windows VST3 preview ZIP on public-repository runs while retaining build, installer and pluginval validation; add an explicit notice and workflow-contract regression tests.
- Expose the existing single default host program as `Default` instead of returning an empty name. Add a portable regression test for the valid and out-of-range indices. No DSP, parameter IDs, presets, automation IDs, or serialized state changed.
- Reject reparse points at package, backup, VST3 bundle and destination roots; walk package/backup trees without following junctions or symbolic links. Add Windows junction regression coverage for fail-closed install and rollback paths.
