# EWAF — Every Week a Folder

EWAF creates one folder for each selected weekday in an inclusive date range. A shared Rust core supplies the behavior; macOS keeps SwiftUI, Windows uses WinUI 3, and Linux uses GTK 4/libadwaita. No cross-platform UI framework is used.

See the [parity matrix](docs/PARITY.md) for automated verification and remaining manual acceptance, and the [audit](docs/history/MIGRATION.md) for the preserved baseline.

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

## Build and run

Install the Rust toolchain selected by rust-toolchain.toml. No Python runtime is needed by a distributed native application.

**macOS 14+**, Apple Silicon/Intel, Swift 6/Xcode 26+:

```sh
./script/build_and_run.sh --verify
./script/build_core.sh
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer swift test
./script/test_ui.sh
UNIVERSAL=1 ./script/package.sh
```

The existing Codex Run button stages/launches `dist/EWAF.app`. Optional modes are `--debug`, `--logs`, `--telemetry`, `--verify`.

**Windows 10 1809+**, x64/ARM64: install Visual Studio Windows development tools, .NET 8 and Rust MSVC, then run:

```powershell
./script/package_windows.ps1 -Architecture x64
./dist/windows-x64/EWAF.exe
```

**Linux**, Ubuntu 24.04 baseline, amd64/arm64:

```sh
sudo apt-get install build-essential pkg-config libgtk-4-dev libadwaita-1-dev libjson-glib-dev desktop-file-utils xvfb dbus-x11
./script/build_linux.sh
GSETTINGS_SCHEMA_DIR="$PWD/build/linux" ./build/linux/ewaf
./script/package_linux.sh
```

## Downloads and installation

Validated [native workflow](https://github.com/tlolabs/ewaf/actions/workflows/native.yml) artifacts and [releases](https://github.com/tlolabs/ewaf/releases) contain versioned packages. macOS: extract the ZIP and move EWAF.app to Applications. Windows: extract the complete ZIP, run EWAF.exe or the included per-user Install.ps1. Linux: install the `.deb` with `sudo apt install ./EWAF-<version>-linux-<architecture>.deb`.

Development macOS artifacts are ad-hoc signed and Windows artifacts can be unsigned while signing credentials are unavailable. First-launch trust prompts depend on OS policy. Stable drafts require owner review. Updates are manual; generated folders and preferences are preserved when replacing the app.

See [behavior](docs/BEHAVIOR.md), [architecture/bindings](docs/ARCHITECTURE.md), [testing](docs/TESTING.md), [packaging/signing/releases](docs/RELEASING.md), and [dependencies](DEPENDENCIES.md).

## Legacy archive

The retired Python applications, tests and dependencies are preserved on [codex/archive-legacy-1.0.2](https://github.com/tlolabs/ewaf/tree/codex/archive-legacy-1.0.2). The original Swift implementation is preserved in that branch’s history. See [archive and recovery](docs/LEGACY_ARCHIVE.md). The active repository contains the Rust/native application and its build tools and regression tests.

## Repository layout

- `crates/`: shared Rust behavior and C ABI, with Rust tests alongside each crate.
- `platform/macos/`, `platform/windows/`, `platform/linux/`: native apps and platform metadata.
- `tests/macos/`, `tests/windows/`, `tests/linux/`: native and integration tests.
- `bindings/`: shared C header and Swift bridge.
- `assets/EWAF.icon`: editable macOS Icon Composer document.
- `assets/icon/`: shared SVG artwork and flat icon exports; see [icon maintenance](assets/icon/README.md).
- `script/`: stable build, run, test and package entry points.
- `docs/`: behavior, architecture, parity and release guidance.

The root Cargo and SwiftPM manifests remain the build entry points. Open `platform/macos/EWAF.xcodeproj` for Xcode. All legacy Python-derived data has been removed from the active tree.
