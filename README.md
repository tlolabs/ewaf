# EWAF — Every Week a Folder

EWAF creates one folder for each selected weekday in an inclusive date range. A shared Rust core supplies the behavior; macOS keeps SwiftUI, Windows uses WinUI 3, and Linux uses GTK 4/libadwaita. No cross-platform UI framework is used.

The native ports are under migration verification. See the [parity matrix](docs/PARITY.md) for actual implementation and test status, and the [audit](docs/MIGRATION.md) for the preserved baseline.

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

**macOS 14+**, Apple Silicon/Intel, Swift 6/Xcode:

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

Validated [native workflow](https://github.com/tlolabs/ewaf/actions/workflows/swift.yml) artifacts and [releases](https://github.com/tlolabs/ewaf/releases) contain versioned packages. macOS: extract the ZIP and move EWAF.app to Applications. Windows: extract the complete ZIP, run EWAF.exe or the included per-user Install.ps1. Linux: install the `.deb` with `sudo apt install ./EWAF-<version>-linux-<architecture>.deb`.

Development macOS artifacts are ad-hoc signed and Windows artifacts can be unsigned while signing credentials are unavailable. First-launch trust prompts depend on OS policy. Stable drafts require owner review. Updates are manual; generated folders and preferences are preserved when replacing the app.

See [behavior](docs/BEHAVIOR.md), [architecture/bindings](docs/ARCHITECTURE.md), [testing](docs/TESTING.md), [packaging/signing/releases](docs/RELEASING.md), and [dependencies](DEPENDENCIES.md).

## Retained Python reference

`legacy-python/` and the original root Python entry points remain runnable and unchanged during migration. They are behavioral references, not native runtime dependencies.

```sh
python3 -m pip install -r legacy-python/requirements.txt
python3 legacy-python/ewaf.py
PYTHONPATH=legacy-python python3 -m unittest discover -s legacy-python/tests -v
```
