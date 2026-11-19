# Changelog

## Unreleased
- Expose the existing single default host program as `Default` instead of returning an empty name. Add a portable regression test for the valid and out-of-range indices. No DSP, parameter IDs, presets, automation IDs, or serialized state changed.
- Reject reparse points at package, backup, VST3 bundle and destination roots; walk package/backup trees without following junctions or symbolic links. Add Windows junction regression coverage for fail-closed install and rollback paths.
