# EWAF — Every Week a Folder

**A TLO Labs open-source project.** EWAF creates one folder for each selected weekday in an inclusive date range. A shared Rust core supplies the behavior; macOS uses SwiftUI, Windows uses WinUI 3, and Linux uses GTK 4/libadwaita.

Maintained by Thomas Lothian. Copyright © Thomas Lothian.

## Downloads and support

[Download the latest release from GitHub](https://github.com/tlolabs/ewaf/releases). Direct distribution and manual updates use GitHub Releases. No automatic update download or installation is implemented.

| Target | Minimum OS | Evidence |
| --- | --- | --- |
| macOS (ARM64/x64) | macOS 14 | Both architectures have CI build/test/package validation; Thomas Lothian primarily tests macOS (ARM64) personally |
| Windows (x64/ARM64) | Windows 10 1809 | Both architectures have CI build/test/package validation; no personal hands-on claim |
| Linux (x64/ARM64) | Ubuntu 24.04 baseline | Both architectures have CI build/test/package validation; interactive accessibility acceptance remains open |

The [parity matrix](docs/PARITY.md) separates implementation, CI evidence and pending manual acceptance. Package formats currently differ from the [Code signing policy](CODE_SIGNING_POLICY.md); see [release status](docs/RELEASING.md) before treating an artifact as production signed.

macOS: extract the ZIP and move EWAF.app to Applications. Windows: extract the whole portable ZIP and run EWAF.exe; optional per-user Install.ps1 and Uninstall.ps1 are included. Linux: the current CI package is a `.deb` for Ubuntu 24.04; install with `sudo apt install ./<downloaded-file>.deb`. AppImage conversion remains release work.

## Use

1. Choose start and end dates and a weekday (Thursday by default).
2. Review the chronological preview or search for a folder date.
3. Choose **Create Folders**, then select an existing destination; selecting the destination first also works.
4. EWAF reports new and existing directories. Existing folders and their contents remain intact.

Names always use ASCII `MM-DD-YYYY`. Exact entry supports years 0001–9999. Both range endpoints are included. Preview shows up to 200 matches; creating uses the entire range even during search. More than 250 folders requires confirmation. Work runs in the background and can be canceled. Fix a reported conflict/permission/storage problem and repeat the same range to safely finish it.

Settings changes the default weekday for new windows. Appearance, typography, scaling and focus follow the native system. Destinations are session-only and must be selected again after restart. Open Folder uses the platform file manager. macOS shares/drags names; Windows/Linux copy/drag names.

| Action | macOS | Windows / Linux |
| --- | --- | --- |
| Choose destination | Command-O | Ctrl-O |
| Create folders | Command-Return | Ctrl-Return |
| Cancel creation | Command-period | Escape |
| Settings | Command-comma | Ctrl-comma |
| New window | Command-N | Ctrl-N |
| Focus search | Native search control | Ctrl-F |

## Build and development

Install the Rust toolchain selected by rust-toolchain.toml. No Python runtime is needed by a distributed native application.

**macOS (ARM64/x64)**, macOS 14+, Swift 6/Xcode 26+:

```sh
./script/build_and_run.sh --verify
./script/build_core.sh
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer swift test
./script/test_ui.sh
UNIVERSAL=1 ./script/package.sh
```

The existing Codex Run button stages/launches `dist/EWAF.app`. Optional modes are `--debug`, `--logs`, `--telemetry`, `--verify`.

**Windows (x64/ARM64)**, Windows 10 1809+: install Visual Studio Windows development tools, .NET 8 and Rust MSVC, then run:

```powershell
./script/package_windows.ps1 -Architecture x64
./dist/windows-x64/EWAF.exe
```

**Linux (x64/ARM64)**, Ubuntu 24.04 baseline:

```sh
sudo apt-get install build-essential pkg-config libgtk-4-dev libadwaita-1-dev libjson-glib-dev desktop-file-utils xvfb dbus-x11
./script/build_linux.sh
GSETTINGS_SCHEMA_DIR="$PWD/build/linux" ./build/linux/ewaf
./script/package_linux.sh
```

Development macOS artifacts are ad-hoc signed and Windows artifacts may be unsigned. First-launch trust prompts depend on OS policy. Replacing the app preserves generated folders and native preferences.

See [building](docs/BUILDING.md), [behavior](docs/BEHAVIOR.md), [architecture](docs/ARCHITECTURE.md), [testing](docs/TESTING.md), [releasing](docs/RELEASING.md), [dependencies](docs/DEPENDENCIES.md) and [changelog](CHANGELOG.md).

## Privacy, security and participation

EWAF performs folder creation offline and has no application telemetry. The Help and Download Updates actions open GitHub in your browser. See [PRIVACY.md](PRIVACY.md) for local settings and logs. Report vulnerabilities through [private reporting](SECURITY.md); use [SUPPORT.md](SUPPORT.md) for other questions. Contributions follow [CONTRIBUTING.md](CONTRIBUTING.md) and the [Contributor Covenant](CODE_OF_CONDUCT.md).

**Licensing:** Original EWAF code, build scripts and the project icon are offered under GPL-3.0-or-later; see [LICENSE](LICENSE) and [icon provenance](assets/icon/README.md). Microsoft and other third-party components retain their own licenses and are not relicensed by EWAF. The [Windows distribution audit](docs/LICENSE_AUDIT.md) distinguishes this project license from the legal status of the current self-contained Windows ZIP and SignPath Foundation eligibility. Third-party terms are preserved in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Legacy archive

The retired Python applications, tests and dependencies are preserved on [codex/archive-legacy-1.0.2](https://github.com/tlolabs/ewaf/tree/codex/archive-legacy-1.0.2). The original Swift implementation is preserved in that branch’s history. See [archive and recovery](docs/LEGACY_ARCHIVE.md). The active repository contains the Rust/native application and its build tools and regression tests.

## Repository layout

- `crates/`: shared Rust behavior and C ABI, with Rust tests alongside each crate.
- `platform/macos/`, `platform/windows/`, `platform/linux/`: native apps and platform metadata.
- `tests/macos/`, `tests/windows/`, `tests/linux/`: native and integration tests.
- `bindings/`: shared C header and Swift bridge.
- `assets/icon/EWAF.icon`: editable macOS Icon Composer document.
- `assets/icon/`: shared SVG artwork and flat icon exports; see [icon maintenance](assets/icon/README.md).
- `script/`: stable build, run, test and package entry points.
- `docs/`: behavior, architecture, parity and release guidance.

The root Cargo and SwiftPM manifests remain the build entry points. Open `platform/macos/EWAF.xcodeproj` for Xcode. All legacy Python-derived data has been removed from the active tree.
