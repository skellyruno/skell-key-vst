# Skell Key repository audit — 2026-11-20

## Scope

Audited skellyruno/skell-key-vst main, source files, README, CMake, repository AGENTS.md, the November 12 Claude handoff export, the SkellCore revision pinned by CMake, open PRs #7–#16, PR file diffs and exact-head GitHub Actions checks.

## Branch and PR findings

Before consolidation:
- main was b66c5999ff9ec2c5fb1d72f35ae8e57319805fad.
- PRs #7–#15 were nine open drafts arranged as a linear stack. The final stack head was 60843d5ea3f64c85b378d98cf94a039a0a13b63f, 18 commits ahead of main and no commits behind.
- PR #15's exact-head Windows run 44695076204 succeeded. The final stack therefore contains the changes from PRs #7–#15 and has a passing exact-head build/test run.
- PR #16 was stacked on #15 and attempted to restore the visible TAG button. It is not part of the consolidated candidate because the user explicitly requested TAG remain hidden.

The nine PRs were consolidated as one review/merge unit instead of merging a chain of overlapping drafts individually. The individual branch history remains available for traceability.

## PR stack audit

| PR | Change | Audit finding |
|---|---|---|
| #7 | Windows x64 PE/archive verification and PR-only CI cancellation | Windows check passed |
| #8 | Flattened VST3 artifact and provenance | Its pluginval check failed due to PowerShell exit-code capture, not a plugin failure |
| #9 | Correct pluginval process exit-code handling | Uses Start-Process and checks the real process exit code; Windows check passed |
| #10 | Strict VST3 moduleinfo normalization | Adds strict normalizer and tests; Windows check passed |
| #11 | Checksum-verified install/update/rollback | Windows check passed; package, backup and VST3 bundle verification added |
| #12 | Non-destructive installer test cleanup | Uses a fresh marked test root and refuses unsafe cleanup; Windows check passed |
| #13 | Valid default VST3 program name | Returns Default for program index 0; adds a portable test; Windows check passed |
| #14 | Reparse-point checks | Adds checks and junction regression cases for package, backup, bundle and destination roots; Windows check passed |
| #15 | Public artifact withholding and CMake cleanup | Withholds installable ZIP on public repo, keeps diagnostic pluginval report, replaces deprecated FetchContent_Populate with FetchContent_MakeAvailable; exact-head Windows run passed |

### PR #8 false failure and correction

The PR #8 log shows pluginval completing all listed tests and printing final SUCCESS, then the workflow failed because $LASTEXITCODE was empty when the GUI-subsystem executable was invoked through PowerShell's call operator. PR #9 changed the invocation to Start-Process -Wait -PassThru and checks Process.ExitCode, while still verifying the generated report ends with SUCCESS and contains no FAILURE. The final PR #15 stack passed this gate.

## Release and installer safety findings

- The staged release validates a single Skell Key.vst3 bundle and the Windows x64 PE32+ module.
- Strict moduleinfo normalization runs on a staged copy, not the build-tree bundle. It checks product/vendor/version, processor/controller classes, unique class IDs and strict JSON; source and staged hashes are recorded.
- The installer verifies package checksums, exact payload coverage, product metadata, VST3 class IDs, AMD64 PE32+ binary, bundle-tree hashes and backup integrity.
- Install/update/rollback is per-user, supports -WhatIf, requires confirmation by default, preserves verified backups and restores the prior bundle if replacement validation fails.
- Installer tests use a newly created, token-marked temporary root. They refuse to claim an existing directory for cleanup and test junction/reparse-point rejection.
- Public repository runs deliberately skip upload of the installable preview ZIP. The pluginval report is a separate diagnostic-only artifact.

The final PR #15 run recorded 33 installer assertions, 18 Python tests, 3 CTests and pluginval strictness 5 SUCCESS. The exact job log reports binary SHA-256 bcd7c712bae8ccb823a490fb2c7ea0c8338f99bf77caef6cf16bb489575ab432; the PR description had a different hash, so the job log is treated as the authoritative record. This does not prove installation or scanning in FL Studio.

## Additional source/documentation issues corrected in the consolidated handoff

- The old README called the current source a “minimal build package from V54” and stated that 22 source files were required. The documentation now describes the actual V56-era source and repository layout.
- The README claimed the localhost WorldLink was “authenticated.” The pinned LocalTuningBus implementation explicitly says it is same-user informational IPC, not an authentication boundary. The README and architecture handoff now state that accurately.
- The old “trusted skell.key peer” receiver guidance applied to Hub peer registration, not to the Windows shared-memory path. The docs distinguish these transports.
- CMake's main branch used deprecated FetchContent_Populate; the release candidate changes to FetchContent_MakeAvailable with SOURCE_SUBDIR SkellCore and keeps the immutable SkellCore pin and SKELLCORE_PATH override.
- The host program name was empty for the sole program; the release candidate provides Default and a regression test.
- Skell Key's Quick4 real-editor renderer remains unverified per repository AGENTS.md. No current-source screenshot is claimed.
- CMake still sets NEEDS_MIDI_INPUT and NEEDS_MIDI_OUTPUT to TRUE. Their host compatibility/necessity remains to be checked before changing them.

## Explicitly excluded change

PR #16 restores the TAG control to the visible editor lifecycle. The V56 README and user's earlier request both say TAG is hidden. It is excluded; do not merge or rebase it into the product unless the user explicitly asks to show TAG again.

## Merge and follow-up policy

- The consolidated release/documentation PR is the single merge target for the #7–#15 stack.
- Merge only after the consolidated PR's exact-head Windows workflow succeeds.
- Do not merge #16.
- Keep accuracy issue #6 open until the original audio/harness and unseen holdout evaluation are available.
- Do not claim a stable release, a current-source UI capture, a successful FL Studio session or an installable public artifact from CI.
