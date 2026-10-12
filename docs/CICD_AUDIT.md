# CI/CD gate audit

Audit date: 2026-10-11

This inventory records repository-controlled gates before policy changes. GitHub
settings were inspected read-only; external organization settings and secret
values were not accessible.

## Repository and GitHub controls

- `main` has no branch protection (`GET /branches/main/protection` returned
  404, “Branch not protected”). The repository rulesets endpoint returned no
  rulesets. The organization rulesets endpoint was unavailable to this
  account, so inherited organization controls cannot be ruled out.
- No repository environments were returned by the environments endpoint.
  The `production` environment named in the release workflow therefore has no
  observed reviewer or wait-timer gate.
- GitHub Actions is enabled and all action sources are allowed. The repository
  permits unsigned commits; `CONTRIBUTING.md` makes DCO sign-off a convention,
  not an enforced check.
- GitHub Dependabot security updates and vulnerability alerts are enabled.
  The repository has a Dependabot configuration; the baseline alert query
  returned zero open alerts. The code-scanning API reported no analysis, and
  no dedicated code-scanning or dependency-audit workflow is present in
  `.github/workflows`. The failure issue records this point-in-time status;
  workflow-run reports do not continuously query alert inventories.
- Repository secrets, variables, organization-level policies and external
  service settings were not inspected or changed.

## Workflow and release gates

- `.github/workflows/native.yml` runs diagnostics on pushes, pull requests,
  tags and manual dispatch. Its `quality` job runs formatting, Clippy, Rust
  tests, updater checks, version/icon/inventory checks and dependency-notice
  verification. Every platform job depends on `quality`, so a failure in any
  shared check prevents all platform builds.
- macOS, Windows and Linux build multiple architectures. macOS icon validation
  depends on both macOS jobs. The release job depends on `quality`, all macOS,
  Windows and Linux jobs, and the internal-only `qt-reference` job. Any one
  failure prevents release assembly and all artifact publication.
- Release artifacts are uploaded only for tags. The release job downloads all
  platform artifacts, assembles one all-platform update manifest, uploads to a
  draft release, and explicitly leaves publication to a separate owner action.
  No automatic bounded retry or consolidated CI-failure issue is configured.
- Stable tags must match the Cargo version, be annotated, be triggered by
  `tlolabs`, and pass production updater-trust preflight. The preflight requires
  an enrolled EWAF update key, Apple and Windows identities, and
  `WINDOWS_DISTRIBUTION_APPROVED=true`.
- The tag release workflow requires macOS Developer ID and notarization
  credentials, Azure Windows signing credentials, and the update-manifest
  signing seed. `script/build_update_release.py` requires all six verified
  platform receipts and signs the manifest and Sparkle feeds. These steps are
  not optional for the existing authenticated updater path.
- `.github/workflows/update-qualification.yml` authenticates a published
  stable release and verifies its attestations. It does not qualify native
  installation and does not gate the preceding release workflow.
- The code-signing policy and release/updater documentation describe owner
  publication, platform signing and full-matrix qualification as mandatory.
  `AGENTS.md` also requires release of all platform artifacts from one commit
  after all required validation. These conflict with the requested
  nonblocking, per-platform publication policy.

## Technical safeguards to preserve

- The installed updater authenticates signed metadata and exact artifact
  digests; macOS additionally uses Sparkle feed/archive signatures and Windows
  verifies Authenticode publisher and MSI identity. Missing trust must fail
  closed. Unsigned artifacts must not be enrolled in or served through this
  updater path.
- Build, package, integrity, launch and platform behavior checks are useful
  diagnostics and should remain visible. A failed artifact must never be
  published as verified.
- The Qt macOS ARM64 package is explicitly internal-only and must remain
  excluded from production release assets.

## Observed limitations

- `updates/trust.json` currently contains no update keys, Apple team ID or
  Windows publisher. No production signing credentials or updater enrollment
  were established by this audit. The checked-in workflow therefore cannot
  produce a verified authenticated stable updater release as configured.
- The workflow required `WINDOWS_DISTRIBUTION_APPROVED=true` based on an
  updater-documentation claim of a Windows hold. The referenced license audit
  records only the retired WinUI concern and documents Qt's compatible
  distribution; no current legal restriction was evidenced. The obsolete
  approval variable was removed.
- The organization rulesets endpoint was inaccessible; any inherited
  organization policy must be confirmed by an administrator. Repository-level
  branch protection and rulesets are absent.
- Direct downloads of unsigned packages may be offered only as an explicitly
  separate supported distribution channel; they do not qualify for the
  authenticated automatic updater.

## Implemented policy changes

- Shared quality, platform builds, and the internal Qt reference now run
  independently. Three direct-release jobs each wait only for their own
  platform matrix and publish successful architecture artifacts with matching
  checksums. Release notes are reconciled afterward. The Qt reference and
  macOS icon recheck do not delay any platform publisher.
- Developer ID/notarization and Azure signing are no longer mandatory for
  direct packages. The authenticated update verification boundary is
  unchanged; the direct release workflow does not create an update manifest.
- Failed native jobs receive one automatic retry. Workflow-run reports update
  the consolidated failure issue with new, persistent and resolved statuses
  per branch or tag, so unrelated passing runs cannot clear proposal failures.
- Repository instructions and release docs now describe nonblocking CI,
  direct per-platform publication and the removal of the stale Windows hold.
- These are implementation facts, not evidence that the updated GitHub
  workflows have run successfully. Per-platform results must come from their
  actual runs after the changes are pushed.
