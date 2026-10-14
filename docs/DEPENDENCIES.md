# Dependencies

The [machine-readable inventory](../dependency-inventory.json) is generated from `Cargo.lock`, `CMakeLists.txt` and declared native build/runtime inputs. `THIRD_PARTY_NOTICES.md` contains locked Rust license texts and Qt 6 LGPL-3.0 notices. Regenerate the inventory/notices when dependencies change. The inventory records package metadata, not a legal compatibility verdict.

| Direct dependency | Purpose | Source and declared license |
| --- | --- | --- |
| Chrono | Gregorian dates | crates.io; MIT or Apache-2.0 |
| cap-std | Directory-relative filesystem access | crates.io; Apache-2.0 with LLVM exception, Apache-2.0 or MIT |
| serde / serde_json | Typed Rust/ABI serialization | crates.io; MIT or Apache-2.0 |
| unicode-normalization / unicode-general-category | Localized date search | crates.io; MIT/Apache-2.0 and Apache-2.0 respectively |
| tempfile | Update staging (tlo-updater) and Rust tests | crates.io; MIT or Apache-2.0 |
| Qt 6 (Core, Gui, Widgets, Concurrent, Test) | One shared presentation for Windows/Linux/internal Mac | Qt Project; LGPL-3.0-only or GPL-3.0-only; see [audit](LICENSE_AUDIT.md) |
| SwiftUI / Foundation | macOS native production UI and local preferences | Apple platform frameworks; platform SDK terms |

Rust toolchain, C++17 compiler, CMake 3.20+ and Python 3 are build inputs. Sparkle is the pinned third-party Swift package. There are no bundled third-party application fonts or telemetry runtimes. Qt 6 is dynamically linked under GNU LGPLv3, which is natively compatible with EWAF's GPL-3.0-or-later license. X11/Wayland, fontconfig and OpenGL remain Linux system libraries. No Avalonia, .NET, WinUI, Windows App SDK, GTK, libadwaita or JSON-GLib application dependency remains. The optional icon generator uses librsvg locally; application builds use committed artwork.

## Rust MSRV policy

The application workspace (`Cargo.toml`) maintains a Minimum Supported Rust Version (MSRV) of **1.85**. The standalone updater crate (`crates/tlo-updater/Cargo.toml`) independently declares an MSRV of **1.89** for its HTTP transport, ring, and cryptographic dependencies. The locked workspace toolchain in CI builds all crates without conflict.

## Packaged native components and Qt provenance

EWAF distributes native platform packages with specific component models:

### Windows (x64 and ARM64)
- **Qt components**: Qt 6.8.0 libraries (`Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Concurrent.dll`) and plugins (`platforms/qwindows.dll`, `styles/qwindowsvistastyle.dll`), built with MSVC 2022.
- **Runtimes**: Microsoft Visual C++ redistributable runtimes (`msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`) bundled under Visual Studio 2022 redistributable terms.
- **Library replacement (LGPLv3 §4)**: Qt libraries are dynamically linked DLLs located in the application directory. Users may replace any Qt DLL with a compatible customized or updated build.

### Linux (.deb and AppImage)
- **Debian package (.deb)**: Dynamically links against system distribution packages (`libqt6core6t64`, `libqt6gui6`, `libqt6widgets6`, `libqt6concurrent6`) on Ubuntu 24.04 LTS (Qt 6.4.2+). Security fixes are maintained via Canonical distribution backports.
- **AppImage**: Bundles Qt 6 shared libraries and the `qxcb` platform plugin, built against Ubuntu 24.04 LTS. The AppImage type 2 runtime components are pinned by exact SHA-256 digests in `updates/build-tools.json`. Users can satisfy LGPLv3 §4 library replacement by extracting the AppImage (`--appimage-extract`), replacing shared objects in `squashfs-root/usr/lib/`, and re-running `appimagetool`.
- **System dependencies**: Wayland/X11 client libraries, fontconfig, OpenGL (Mesa), and dconf remain unbundled system dependencies.

### macOS (Apple Silicon and Intel)
- **Production UI**: Native SwiftUI and AppKit; no Qt runtime is bundled in production.
- **Bundled frameworks**: Sparkle 2.9.6 framework for authenticated updates, pinned by commit/archive digest.
- **Internal reference**: Internal macOS Qt reference dynamically links Homebrew Qt 6 for non-production parity comparison only; it is never published to users.

## Updater dependencies

`Cargo.lock` pins the standalone updater's ring, SHA-256, SemVer and HTTPS transport dependencies. `tempfile` is utilized during update staging for atomic transactional swaps. Sparkle 2.9.6 is pinned by `Package.resolved` and an independent archive digest in `script/prepare_sparkle.sh`; its complete upstream license accompanies Rust notices. WiX 4.0.6 is a Windows build tool for MSI packaging. Linux build-tool and AppImage runtime digests are recorded in `updates/build-tools.json`; they are build inputs, not application update channels. The generated dependency inventory records these sources. Neither updater crate depends on AVID Core.
