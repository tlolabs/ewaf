# Build EWAF

The application version is read from the workspace `Cargo.toml`. Use the pinned Rust toolchain in `rust-toolchain.toml`; commit both Rust and Windows package locks. Build prerequisites and commands follow each native UI.

| Target | Minimum supported OS | Build requirements | Entry point |
| --- | --- | --- | --- |
| macOS (ARM64/x64) | macOS 14 | Xcode 26+, Swift 6, Rust | `./script/build_and_run.sh --verify` |
| Windows (x64/ARM64) | Windows 10 1809 | Visual Studio Windows tools, .NET 8, Rust MSVC | `./script/package_windows.ps1 -Architecture x64` or `ARM64` |
| Linux (x64/ARM64) | Ubuntu 24.04 baseline | GTK 4.10+, libadwaita 1.4+, json-glib, pkg-config, C toolchain, Rust | `./script/build_linux.sh` |

For macOS Swift tests, run `./script/build_core.sh` **before** `swift test`; this refreshes the Rust archive/build stamp. `./script/test_ui.sh` runs the Xcode UI harness. On Linux, install the native development packages listed in [TESTING.md](TESTING.md); `./script/test_linux.sh` builds the GTK widget tests. On Windows, run the packaged C# and UI tests described there.

`./script/package.sh`, `./script/package_windows.ps1` and `./script/package_linux.sh` currently produce the packages described in [RELEASING.md](RELEASING.md). Development builds must work without production credentials. Do not check generated binaries, build stamps or credentials into Git.
