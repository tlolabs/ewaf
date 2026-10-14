# Dependencies

The [machine-readable inventory](../dependency-inventory.json) is generated from `Cargo.lock`, `CMakeLists.txt` and declared native build/runtime inputs. `THIRD_PARTY_NOTICES.md` contains locked Rust license texts and Qt 6 LGPL-3.0 notices. Regenerate the inventory/notices when dependencies change. The inventory records source declarations, not observed package contents or a legal compatibility verdict. Schema 4 labels these as `declared_native_components`; it does not certify vendor security patches, exact transitive libraries, or bundled license completeness.

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

The application workspace (`Cargo.toml`) maintains a Minimum Supported Rust Version (MSRV) of **1.85**. The standalone updater crate (`crates/tlo-updater/Cargo.toml`) independently declares an MSRV of **1.89** and uses the standard library file-locking API. This does not establish the minimum version of every transitive dependency. The locked workspace toolchain in CI builds all crates without conflict.

## Packaged native components and Qt provenance

The following are declared build inputs and expected component models. Confirm each actual package and architecture with the inspection procedure below.

### Windows (x64 and ARM64)
- **Declared Qt input**: `.github/workflows/build-windows.yml` selects Qt 6.8.0. Expected libraries (`Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Concurrent.dll`) and plugins (`platforms/qwindows.dll`, `styles/qwindowsvistastyle.dll`), built with MSVC 2022.
- **Expected runtimes**: Microsoft Visual C++ redistributable runtimes (`msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`) bundled under Visual Studio 2022 redistributable terms.
- **Library replacement (LGPLv3 §4)**: Qt libraries are dynamically linked DLLs located in the application directory. Users may replace any Qt DLL with a compatible customized or updated build.

### Linux (.deb and AppImage)
- **Debian package (.deb)**: Dynamically links against system distribution packages (`libqt6core6t64`, `libqt6gui6`, `libqt6widgets6`, `libqt6concurrent6`) on Ubuntu 24.04 LTS (Qt 6.4.2+). Security fixes are maintained via Canonical distribution backports.
- **AppImage**: Bundles Qt 6 shared libraries and the `qxcb` platform plugin, built against Ubuntu 24.04 LTS. The AppImage type 2 runtime components are pinned by exact SHA-256 digests in `updates/build-tools.json`. Users can satisfy LGPLv3 §4 library replacement by extracting the AppImage (`--appimage-extract`), replacing shared objects in `squashfs-root/usr/lib/`, and re-running `appimagetool`.
- **System versus bundled libraries**: Debian uses distribution dependencies. AppImage dependency closure is determined by the actual `linuxdeploy` output; do not assume X11, fontconfig or other libraries remain unbundled. dconf is used for preference import.

### macOS (Apple Silicon and Intel)
- **Production UI**: Native SwiftUI and AppKit; no Qt runtime is bundled in production.
- **Bundled frameworks**: Sparkle 2.9.6 framework for authenticated updates, pinned by commit/archive digest.
- **Internal reference**: Internal macOS Qt reference dynamically links Homebrew Qt 6 for non-production parity comparison only; it is never published to users.

## Updater dependencies

`Cargo.lock` pins the standalone updater's ring, SHA-256, SemVer and HTTPS transport dependencies. `tempfile` is utilized during update staging for atomic transactional swaps. Sparkle 2.9.6 is pinned by `Package.resolved` and an independent archive digest in `script/prepare_sparkle.sh`; its complete upstream license accompanies Rust notices. WiX 4.0.6 is a Windows build tool for MSI packaging. Linux build-tool and AppImage runtime digests are recorded in `updates/build-tools.json`; they are build inputs, not application update channels. The generated dependency inventory records these sources. Neither updater crate depends on AVID Core.

## Inspect actual packages separately

`script/inspect_native_package.py` reads an already extracted local package without loading or executing its application, installer, plugins, or AppImage. Run it separately for each platform and architecture, for example:

```sh
python3 script/inspect_native_package.py --tree dist/windows-x64 --platform windows --arch x64 --package-file dist/ewaf-1.0.5-windows-x64.msi --output build/windows-x64-native-inventory.json
python3 script/inspect_native_package.py --tree build/extracted-appimage --platform linux --arch arm64 --package-file dist/ewaf-1.0.5-linux-arm64.AppImage --runtime-file build/pinned-runtime-aarch64 --output build/linux-arm64-native-inventory.json
python3 script/inspect_native_package.py --tree dist/EWAF.app --platform macos --arch arm64 --package-file dist/EWAF-1.0.5-macos-arm64.zip --output build/macos-arm64-native-inventory.json
```

The tree is a caller-supplied extraction. A package hash alone does not prove tree/package correspondence: retain extraction commands and logs, or compare against the packaging staging tree. For MSI use administrative/offline extraction in an isolated Windows fixture; for AppImage use SquashFS inspection/extraction tools, never an unknown executable's `--appimage-extract` entry point. The inspector records every file's SHA-256 and contained symlink, actual plugins/runtimes, PE architecture and RT_VERSION where present, ELF architecture and `readelf` dependency entries, and Mach-O load commands and bundle versions. Missing native inspection tools and version records remain explicit unknowns. Supplying the pinned AppImage runtime additionally compares its digest and its bytes against the package prefix. It does not infer patch provenance from an ELF SONAME or filename.

Reconcile the observed binaries against vendor source versions, distribution package revisions/changelogs, Qt-provided SBOMs, bundled notices, and source availability/replacement instructions. Qt's library replacement mechanism must cover all distributed shared libraries/plugins and include applicable notices and corresponding source information. AppImage users can run the extracted `AppRun` with compatible replaced libraries; rebuilding the image is optional. Source inventory generation alone cannot establish that these obligations have been satisfied. Windows compiler-runtime revisions and license files must be checked per architecture. Full Windows/Linux package receipts remain pending.

## Qt security provenance limitations

Qt 6.8 is an LTS series, but selecting the original 6.8.0 binary does not install later security fixes or provide commercial LTS patch access. Qt 6.4 is not an upstream LTS series; Ubuntu 24.04's distribution lifetime is a separate matter. Capture the exact Ubuntu package revision and published security/backport evidence for each included binary rather than asserting all relevant fixes are present. Homebrew Qt changes independently; local verification on 2026-10-14 used Qt 6.11.2 for the internal macOS reference, which supplies no evidence about production Windows/Linux binaries.

Not linking QtNetwork/WebEngine reduces exposure to their APIs but does not establish immunity for QtCore/QtGui or image/platform plugins. Qt's [security policy and SBOM guidance](https://doc.qt.io/qt-6.8/security.html), [release/support table](https://doc.qt.io/qt-6.10/qt-releases.html), and [QColorTransferGenericFunction advisory (CVE-2025-5992)](https://www.qt.io/blog/security-advisory-recently-reported-denial-of-service-issue-in-qcolortransfergenericfunction-impacts-qt) demonstrate the need for a component-specific advisory review. The latter is an example affecting QtGui; applicability to EWAF's actual input paths still requires evidence. This review does not claim a complete vulnerability inventory or exploitability verdict.

**DEP-03 remains unresolved for production Qt artifacts.** Obtain verified vendor/backport provenance or select an updated Qt build and validate both Windows architectures and the supported minimum OS. An upgrade that drops Windows 10 1809 or another supported configuration requires owner approval. No runtime dependency or minimum OS was silently upgraded by the independent verification.
