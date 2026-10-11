# Releases

GitHub Releases are EWAF's canonical direct-download source. `Cargo.toml`
owns the version; stable tags use `vMAJOR.MINOR.PATCH` and must match it.
Do not invent versions or retag an existing release. DCO sign-off is a
contribution convention; commit and tag signatures, pull requests, reviews,
status checks and manual release approval are not required.

| Channel | Trigger | Publication |
| --- | --- | --- |
| Stable | Exact `vMAJOR.MINOR.PATCH` matching Cargo | Published GitHub Release |
| Prerelease | Matching `vMAJOR.MINOR.PATCH-beta.N` or `-rc.N` tag | Published GitHub prerelease |
| Development | Push to `main` | Successful platform build artifacts, retained 30 days |

No nightly channel is currently configured. Do not invent a nightly version or
tag convention; adding one requires an established versioning rule.

## Nonblocking CI and independent publication

`.github/workflows/native.yml` runs shared quality checks and each platform
matrix independently on pushes, pull requests, tags and manual dispatch.
Shared checks, the additional macOS icon recheck and the internal Qt reference
are diagnostic jobs, not dependencies of platform builds. The tag publisher
runs even when diagnostics or another platform fail. It collects only
artifacts uploaded after their own platform tests pass, verifies their
SHA-256 sidecars, excludes the internal Qt reference, and publishes successful
platform files to the existing or newly created release. Existing assets are
not duplicated or overwritten. A platform that later passes can add its
missing artifact to the same release without changing the version.

The Windows MSI is built and tested unsigned. The historical Windows
distribution concern in [`LICENSE_AUDIT.md`](LICENSE_AUDIT.md) was resolved
when the retired WinUI application was replaced by the shared Qt
presentation; no approval variable is required. CI records a checksum and
publishes the MSI after the Windows package and functional checks pass.

### Platform evidence required before upload

| Platform | Build and package | Functional/integrity evidence | Direct-release artifact |
| --- | --- | --- | --- |
| macOS ARM64/x64 | Swift/Rust builds, app bundle and ZIP validation | Rust and Swift tests, native UI tests, package validation, Sparkle interoperation, SHA-256 | macOS ZIP and checksum |
| Windows x64/ARM64 | Qt/Rust application and MSI | Shared FFI tests, native updater rejection test, MSI upgrade/rollback, accessibility/UI and install/removal tests, SHA-256 | MSI and checksum |
| Linux x64/ARM64 | Qt/Rust `.deb` and AppImage | Shared UI tests, packaged `.deb` launch/core smoke, AppImage package script core/UI smoke, SHA-256 | AppImage and checksum |

Only tests that actually ran are evidence. Developer ID signing, notarization,
Azure Artifact Signing and Authenticode are not publication prerequisites.
macOS development packages retain an ad-hoc code signature for local package
integrity; they are not Developer ID signed or notarized. OS trust prompts may
therefore apply. GitHub build attestations are provenance diagnostics, not a
replacement for platform functionality or updater signatures.

## Direct downloads and authenticated updates

Unsigned/unnotarized direct-download releases do not contain a trusted
`update-manifest.json` and are not automatic-updater releases. The
`update-qualification.yml` workflow skips release-published events when that
manifest is absent. No code path weakens the installed updater: macOS Sparkle
feed/archive verification, Ed25519 update-manifest signatures and exact
artifact digests, and Windows publisher/MSI verification remain mandatory.
`updates/trust.json` is currently unenrolled, so authenticated update
publication is unavailable. Keep
[`UPDATING.md`](UPDATING.md) and
[`CODE_SIGNING_POLICY.md`](../CODE_SIGNING_POLICY.md) authoritative for this
boundary.

The signing and update assembly tools remain available for a future
authenticated channel: `script/package.sh` accepts optional
`SIGNING_IDENTITY` and `NOTARY_PROFILE`; Windows packaging retains optional
local certificate signing; `script/update_receipt.py`,
`script/check_update_trust.py` and `script/build_update_release.py` validate
platform identities and create signed manifests. Do not send unsigned
artifacts through those tools or serve them as authenticated updates.
Reintroducing authenticated publication requires a separately reviewed,
nonblocking workflow path that runs only when the required trust material is
enrolled and preserves fail-closed verification.

## Retry and failure reporting

The native CI retry workflow reruns failed jobs once. A passing retry changes
the recorded status; it does not hide the original failure or convert a failed
check into success. Subsequent commits also run the full independent matrix.
The CI/CD failure-report workflow updates one consolidated GitHub issue after
native and updater-qualification runs, distinguishes new, persistent and
resolved jobs per branch or tag, and keeps external restrictions visible. A
passing `main` run does not clear a failing proposal. These reports never
gate builds or publication. GitHub Dependabot alerts and security updates are
enabled; the repository has no separate code-scanning workflow.

The repository audit and live GitHub settings observations are recorded in
[`CICD_AUDIT.md`](CICD_AUDIT.md) and [`GITHUB.md`](GITHUB.md). The organization
rulesets endpoint could not be inspected with available API access; no
repository branch protection or rulesets were found.

## Build commands and distribution notes

- macOS: `UNIVERSAL=1 ./script/package.sh` after
  `./script/build_core.sh`. Minimum macOS is 14. Optional signing inputs are
  read from the local Keychain/environment.
- Windows: `./script/package_windows.ps1 -Architecture x64` or `ARM64` on the
  corresponding Windows runner. It stages the shared Qt application, native
  updater/installer tools and Qt runtime libraries. MSI packaging and
  installation tests run separately.
- Linux: `./script/package_linux.sh` builds the Ubuntu `.deb`;
  `./script/package_appimage.sh` builds and smoke-tests the AppImage using
  checksum-pinned packaging tools. Qt 6 and X11/Wayland remain system
  dependencies.

The Windows portable ZIP still includes per-user `Install.ps1` and
`Uninstall.ps1`. The Linux `.deb` is removed with the package manager. Removing
the macOS app does not delete generated folders or preferences. Never commit
generated binaries or signing credentials.

The Qt reference is internal-only, has identity
`com.tlolabs.ewaf.qt-internal`, and is excluded from `EWAF-*` production
artifacts. Never rename it to a production package. The versioned release
history documents prior artifact formats; do not infer current availability
from historical entries.
