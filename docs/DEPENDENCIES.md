# Dependencies

The [machine-readable inventory](../dependency-inventory.json) is generated from `Cargo.lock`, `CMakeLists.txt` and declared native build/runtime inputs. `THIRD_PARTY_NOTICES.md` contains locked Rust license texts and Qt 6 LGPL-3.0 notices. Regenerate the inventory/notices when dependencies change. The inventory records package metadata, not a legal compatibility verdict.

| Direct dependency | Purpose | Source and declared license |
| --- | --- | --- |
| Chrono | Gregorian dates | crates.io; MIT or Apache-2.0 |
| cap-std | Directory-relative filesystem access | crates.io; Apache-2.0 with LLVM exception, Apache-2.0 or MIT |
| serde / serde_json | Typed Rust/ABI serialization | crates.io; MIT or Apache-2.0 |
| unicode-normalization / unicode-general-category | Localized date search | crates.io; MIT/Apache-2.0 and Apache-2.0 respectively |
| tempfile | Rust tests only | crates.io; MIT or Apache-2.0 |
| Qt 6 (Core, Gui, Widgets, Concurrent, Test) | One shared presentation for Windows/Linux/internal Mac | Qt Project; LGPL-3.0-only or GPL-3.0-only; see [audit](LICENSE_AUDIT.md) |
| SwiftUI / Foundation | macOS native production UI and local preferences | Apple platform frameworks; platform SDK terms |

Rust toolchain, C++17 compiler, CMake 3.20+ and Python 3 are build inputs. Sparkle is the pinned third-party Swift package. There are no bundled third-party application fonts or telemetry runtimes. Qt 6 is dynamically linked under GNU LGPLv3, which is natively compatible with EWAF's GPL-3.0-or-later license. X11/Wayland, fontconfig and OpenGL remain Linux system libraries. No Avalonia, .NET, WinUI, Windows App SDK, GTK, libadwaita or JSON-GLib application dependency remains. The optional icon generator uses librsvg locally; application builds use committed artwork.

## Updater dependencies

`Cargo.lock` pins the standalone updater's ring, SHA-256, SemVer and HTTPS transport dependencies. Sparkle 2.9.6 is pinned by `Package.resolved` and an independent archive digest in `script/prepare_sparkle.sh`; its complete upstream license accompanies Rust notices. WiX 4.0.6 is a Windows build tool for MSI packaging. Linux build-tool and AppImage runtime digests are recorded in `updates/build-tools.json`; they are build inputs, not application update channels. The generated dependency inventory records these sources. Neither updater crate depends on AVID Core.
