# Packaging, CI and releases

`Cargo.toml` owns the application version. Native copies are checked by `script/check_versions.py`; generated Xcode metadata reads it directly. Bump workspace version, Windows Directory.Build.props and app.manifest together, regenerate Xcode and lockfiles, and run CI. Platform build numbers may differ; the feature version and commit must agree.

## macOS

`UNIVERSAL=1 ./script/package.sh` builds Rust for Apple Silicon and Intel, combines the static library, builds the universal SwiftUI app, stages `dist/EWAF.app`, ad-hoc signs it by default and produces a versioned ZIP/checksum. Without UNIVERSAL, the host architecture is packaged. `APP_VERSION` overrides are rejected unless they match the core. The bundle remains `com.tlolabs.ewaf`, minimum macOS 14.

Optional `SIGNING_IDENTITY` enables Developer ID/hardened runtime. `NOTARY_PROFILE` uses an existing Keychain notarytool profile, waits for notarization, staples/validates and repackages. No identity or account is created by the scripts. Keep credentials in Keychain or CI secrets, never source control.

## Windows

Run `./script/package_windows.ps1 -Architecture x64` or `ARM64` in PowerShell on Windows. It builds the matching Rust DLL, publishes a self-contained WinUI 3 application, and creates a versioned ZIP/checksum. Artifact validation checks executable/DLL PE architecture and required PRI/XBF resources. The ZIP includes Install.ps1 and Uninstall.ps1 for a per-user Programs/Start-menu installation without administrator privileges; direct launch from the extracted folder also works. A package-owned file manifest limits uninstall to shipped files, preserving preferences and generated folders, including empty folders placed inside the installation directory. CI tests installation and removal against both architectures. Close EWAF before replacing an installed version.

Optional `WINDOWS_CERTIFICATE_PATH` and `WINDOWS_CERTIFICATE_PASSWORD` enable Authenticode signing with signtool and verification. Supply a certificate from a secure CI temporary file or an approved local signing setup. CI accepts WINDOWS_CERTIFICATE_P12_BASE64 and WINDOWS_CERTIFICATE_PASSWORD secrets for stable tags and removes the temporary certificate afterward. Missing credentials produce unsigned development builds. Windows may show trust warnings until certificates/reputation are available; do not disable system protections globally.

## Linux

`./script/package_linux.sh` builds against Ubuntu 24.04 native dependencies and produces an amd64 or arm64 `.deb`. It installs the executable, desktop entry and GSettings schema and includes schema-cache maintenance scripts. Validate desktop metadata and schema syntax before packaging; CI installs the actual package and checks executable/schema availability. Distribution libraries remain dynamic. A Debian repository should sign Release/InRelease metadata with an owner-controlled key; standalone checksum files provide integrity comparison, not authenticated publisher identity. No repository signing key is currently configured.

## CI and release gates

Native platforms (`.github/workflows/swift.yml`) runs Rust quality/tests and native build/test/package jobs on macOS arm64/Intel, Windows x64/ARM64 and Ubuntu 24.04 amd64/arm64. Archived Python application CI has been retired; Python build scripts remain part of native packaging. Required tests precede artifact upload. Main builds publish commit-specific development prereleases only after all native jobs succeed. Tagged `v<workspace-version>` builds prepare draft stable releases after those same gates. Stable publication remains owner review. This provides development builds on every successful main update without an idle nightly rebuild.

Every release artifact comes from the same checked-out SHA. No platform can publish a partial release after another platform fails. ZIP/.deb validation and SHA-256 files are mandatory. macOS signing acts on already-tested downloaded artifacts rather than rebuilding a different revision. Review actual CI results and record manual acceptance or an explicit owner release decision before publishing a stable draft. Unperformed checks must remain documented.

Existing optional macOS secrets are preserved: MACOS_CERTIFICATE_P12_BASE64, MACOS_CERTIFICATE_PASSWORD, MACOS_KEYCHAIN_PASSWORD, MACOS_SIGNING_IDENTITY, NOTARY_APPLE_ID, NOTARY_APP_PASSWORD and APPLE_TEAM_ID. Temporary Keychains and certificate files are removed in an always step. No paid account or signing identity is required for development.

## Updates and troubleshooting

Use the app's Download Updates link to find builds, quit the app and install the replacement. There is no unattended updater. On macOS, inspect `codesign --verify --strict dist/EWAF.app`; on Windows, check DLL architecture and whether the complete self-contained publish folder was extracted; on Linux, check native package dependencies and `gsettings list-keys com.tlolabs.ewaf`. Missing schema errors in a local source run are resolved with `GSETTINGS_SCHEMA_DIR=$PWD/build/linux` after building. For Windows startup diagnostics, set `EWAF_DIAGNOSTICS_PATH` to a writable temporary log path before launch; the app writes resource/window initialization checkpoints and unhandled exceptions only when that variable is supplied. Core errors are returned to native status/dialogs; --logs/--telemetry streams macOS process logs and does not add tracking.
