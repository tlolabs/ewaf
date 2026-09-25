# Releases

GitHub Releases are EWAF's canonical direct-download source. Version strings use `MAJOR.MINOR.PATCH` in Cargo and native metadata; stable tags use `vMAJOR.MINOR.PATCH`. Do not retag or rewrite earlier releases. The existing `v1.0.4` tag is unsigned; it is historical and does **not** meet the new stable policy.

## Current implementation and release hold

`Cargo.toml` owns the current version. `script/check_versions.py` checks Windows copies, and macOS metadata reads the workspace version. CI builds/tests the six native architecture targets and checks the dependency inventory and notices. The tag job rejects a mismatched version, missing root `LICENSE`, lightweight tag or tag without GitHub-verified signature attributed to Thomas Lothian. The source license is now GPL-3.0-or-later, but [the Windows distribution audit](LICENSE_AUDIT.md) leaves the current self-contained Windows ZIP unapproved for a new release pending exact license/SignPath review. The existing workflow's package/signing path is **not** yet a fully compliant stable release system; do not bypass this hold or publish its draft as production validated.

Current package scripts build an ad-hoc or optionally Developer ID signed macOS ZIP, a Windows portable ZIP with optional PFX signing, and an Ubuntu `.deb`. The current workflow waits for all platform jobs, uses optional signing credentials, and does not independently publish successful platforms after another platform fails. It does not yet generate release SBOMs, attestations, a complete `SHA256SUMS`, Azure Artifact Signing or a signed AppImage. These are open implementation items, not completed features. Existing `.deb` packaging remains for users until an AppImage passes native validation.

## Target channels and packages

| Channel | Trigger | Signing | Distribution |
| --- | --- | --- | --- |
| Stable | Thomas Lothian's verified signed `vMAJOR.MINOR.PATCH` tag | Production methods below, mandatory | GitHub Release |
| Prerelease | `vMAJOR.MINOR.PATCH-beta.N` or `-rc.N` | No production signing | Clearly marked GitHub prerelease |
| Development | `main` | No production signing | Clearly marked development artifact, retained 30 days; runnable macOS (ARM64) package by default |

Normalized filenames are `ewaf-<version>-macos-arm64.zip`, `ewaf-<version>-macos-x64.zip`, `ewaf-<version>-windows-x64.zip`, `ewaf-<version>-windows-arm64.zip`, `ewaf-<version>-linux-x64.AppImage` and `ewaf-<version>-linux-arm64.AppImage`. Product UI stays `EWAF`. Existing package scripts still use older names/formats; the target names must not be advertised as available until implemented.

For stable releases, each platform should build and pass its own required tests before signing and publication. macOS requires Developer ID signature verification, notarization, stapling and a completed ZIP. Windows requires Azure Artifact Signing, Authenticode verification and a portable ZIP. Linux requires a GPG-signed AppImage. Each released artifact should have an SBOM, SHA-256 entry in `SHA256SUMS`, and GitHub/Sigstore attestation where supported. Sign the checksum manifest where appropriate. If a platform fails, publish no artifact for that platform; the release notes must name the missing expected artifact while successful platforms may proceed. The user must choose any update download and installation.

Suggested release-note sections are Highlights, Changes, Fixes, Known issues, Supported platforms, Installation/update notes and Verification/security information; omit empty sections. Record CI results, personally tested targets and unresolved manual acceptance separately. See [TESTING.md](TESTING.md) and [PARITY.md](PARITY.md).

## Current build commands

Run `UNIVERSAL=1 ./script/package.sh` on macOS after `./script/build_core.sh`; the bundle identifier remains `com.tlolabs.ewaf` and the minimum OS remains macOS 14. `SIGNING_IDENTITY` and `NOTARY_PROFILE` are optional local Keychain inputs. Run `./script/package_windows.ps1 -Architecture x64` or `ARM64` on the respective Windows runners. It uses locked NuGet restore, a self-contained publish and vendor notice collection; the current optional PFX path is development/legacy release plumbing, not Azure Artifact Signing. Run `./script/package_linux.sh` on Ubuntu 24.04 for the current `.deb`; GTK/libadwaita are dynamic distribution dependencies. Never commit signing credentials or generated binaries.

The Windows ZIP contains per-user Install.ps1 and Uninstall.ps1; direct launch also works. The current Linux `.deb` can be removed using the platform package manager. Removing the macOS app does not delete generated folders or preferences. For diagnostics, set `EWAF_DIAGNOSTICS_PATH` on Windows or use the macOS local `--logs` mode. See [PRIVACY.md](../PRIVACY.md).
