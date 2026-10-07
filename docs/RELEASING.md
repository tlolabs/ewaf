# Releases

GitHub Releases are EWAF's canonical direct-download source. Version strings use `MAJOR.MINOR.PATCH` in Cargo and native metadata; stable tags use `vMAJOR.MINOR.PATCH`. Do not routinely retag or rewrite earlier releases. The owner-authorized [unsigned-commit migration](history/UNSIGNED_COMMITS.md) is a one-time exception for removing historical commit signatures while preserving source snapshots and messages; its record distinguishes local changes from published history. The existing `v1.0.4` release is historical and has no authenticated updater; changing the tag policy does not retroactively qualify its artifacts. Version 1.0.5 is the next candidate; no new stable tag has been created.

## Authenticated update release pipeline

The new updater pipeline and exact outstanding gates are documented in [UPDATING.md](UPDATING.md). Stable update artifacts are macOS ZIP, Windows MSI and Linux AppImage, with signed manifests/appcasts and mandatory platform verification receipts. The six-platform matrix must pass on one commit. The older descriptions below record the pre-updater packaging baseline; they do not authorize optional production signing. Production remains blocked until trust enrollment, the existing Windows distribution hold and native updater qualification are resolved.

## Previous implementation and release hold

`Cargo.toml` owns the current version. `script/check_versions.py` checks Windows copies, and macOS metadata reads the workspace version. CI builds/tests the six native architecture targets and checks the dependency inventory and notices. The tag job rejects a mismatched version, missing root `LICENSE`, lightweight tag or tag triggered by an account other than Thomas Lothian. The source license is GPL-3.0-or-later. Qt 6 is dynamically linked under GNU LGPLv3. A stable release still requires public update trust, production signing enrollment, the existing Windows distribution approval gate and signed older-to-newer update qualification. Do not bypass these holds or publish a draft as production validated.

Current package scripts build an ad-hoc or optionally Developer ID signed macOS ZIP, a Windows portable ZIP with optional PFX signing, and an Ubuntu `.deb`. The current workflow waits for all platform jobs, uses optional signing credentials, and does not independently publish successful platforms after another platform fails. It does not yet generate release SBOMs, attestations, a complete `SHA256SUMS`, Azure Artifact Signing or an updater-qualified AppImage. These are open implementation items, not completed features. Existing `.deb` packaging remains for users until an AppImage passes native validation.

## Target channels and packages

| Channel | Trigger | Signing | Distribution |
| --- | --- | --- | --- |
| Stable | Thomas Lothian's annotated `vMAJOR.MINOR.PATCH` tag | Production methods below, mandatory | GitHub Release |
| Prerelease | `vMAJOR.MINOR.PATCH-beta.N` or `-rc.N` | No production signing | Clearly marked GitHub prerelease |
| Development | `main` | No production signing | Clearly marked development artifact, retained 30 days; runnable macOS (ARM64) package by default |

Normalized filenames are `ewaf-<version>-macos-arm64.zip`, `ewaf-<version>-macos-x64.zip`, `ewaf-<version>-windows-x64.zip`, `ewaf-<version>-windows-arm64.zip`, `ewaf-<version>-linux-x64.AppImage` and `ewaf-<version>-linux-arm64.AppImage`. Product UI stays `EWAF`. Existing package scripts still use older names/formats; the target names must not be advertised as available until implemented.

For stable releases, each platform should build and pass its own required tests before signing and publication. macOS requires Developer ID signature verification, notarization, stapling and a completed ZIP. Windows requires Azure Artifact Signing, Authenticode verification and a portable ZIP. Linux requires a same-commit AppImage validated against its signed Ed25519 update manifest and exact digest, plus build attestation. Each released artifact should have an SBOM, SHA-256 entry in `SHA256SUMS`, and GitHub/Sigstore attestation where supported. Sign the checksum manifest where appropriate. If a platform fails, publish no artifact for that platform; the release notes must name the missing expected artifact while successful platforms may proceed. The user must choose any update download and installation.

Suggested release-note sections are Highlights, Changes, Fixes, Known issues, Supported platforms, Installation/update notes and Verification/security information; omit empty sections. Record CI results, personally tested targets and unresolved manual acceptance separately. See [TESTING.md](TESTING.md) and [PARITY.md](PARITY.md).

## Current build commands

Run `UNIVERSAL=1 ./script/package.sh` on macOS after `./script/build_core.sh`; the bundle identifier remains `com.tlolabs.ewaf` and the minimum OS remains macOS 14. `SIGNING_IDENTITY` and `NOTARY_PROFILE` are optional local Keychain inputs. Run `./script/package_windows.ps1 -Architecture x64` or `ARM64` on the respective Windows runners. It builds the Qt application with CMake, stages `ewaf_ffi.dll` and `ewaf-update.exe`, deploys Qt runtime libraries with `windeployqt`, and packages the ZIP; the current optional PFX path is development/legacy release plumbing, not Azure Artifact Signing. Run `./script/package_linux.sh` on Ubuntu 24.04 for the `.deb` and `./script/package_appimage.sh` for AppImage; Qt 6 and X11/Wayland are system libraries. Never commit signing credentials or generated binaries.

The Windows ZIP contains per-user Install.ps1 and Uninstall.ps1; direct launch also works. The current Linux `.deb` can be removed using the platform package manager. Removing the macOS app does not delete generated folders or preferences. For diagnostics, set `EWAF_DIAGNOSTICS_PATH` on Windows or use the macOS local `--logs` mode. See [PRIVACY.md](../PRIVACY.md).

## Internal artifact exclusion

`qt-reference` is a required shared-presentation CI job, including on release tags, but its `internal-qt-reference-osx-arm64` upload is excluded from the `EWAF-*` production artifact download pattern. Release assembly rejects any internal/qt-internal-named input before signing. Only native SwiftUI bundles with `com.tlolabs.ewaf` pass production macOS validation/receipt generation. The internal bundle has `com.tlolabs.ewaf.qt-internal`, no Sparkle/feed metadata or helper, and both a compile-time internal flag and runtime updater rejection. Never rename an internal artifact to a production filename.
