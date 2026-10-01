# Dependencies

The [machine-readable inventory](../dependency-inventory.json) is generated from `Cargo.lock`, `platform/avalonia/EWAF/packages.lock.json` and declared native build/runtime inputs. `THIRD_PARTY_NOTICES.md` contains locked Rust license texts; Windows packages gather NuGet and .NET vendor notices. Regenerate the inventory/notices when dependencies change. The inventory records package metadata, not a legal compatibility verdict.

| Direct dependency | Purpose | Source and declared license |
| --- | --- | --- |
| Chrono | Gregorian dates | crates.io; MIT or Apache-2.0 |
| cap-std | Directory-relative filesystem access | crates.io; Apache-2.0 with LLVM exception, Apache-2.0 or MIT |
| serde / serde_json | Typed Rust/ABI serialization | crates.io; MIT or Apache-2.0 |
| unicode-normalization / unicode-general-category | Localized date search | crates.io; MIT/Apache-2.0 and Apache-2.0 respectively |
| tempfile | Rust tests only | crates.io; MIT or Apache-2.0 |
| Avalonia.Desktop / Avalonia.Themes.Fluent | One shared presentation for Windows/Linux/internal Mac | 12.1.3; MIT plus upstream component notices, including Ms-PL; see [audit](LICENSE_AUDIT.md) |
| SwiftUI / Foundation | macOS native UI and local preferences | Apple platform frameworks; platform SDK terms |
| SkiaSharp / HarfBuzzSharp / ANGLE | Rendering, text shaping and Windows graphics | 3.119.4 / 8.3.1.3 / 2.1.27548.20260419; MIT/BSD and bundled third-party notices |
| .NET 10 | Shared self-contained desktop and installer runtime | SDK 10.0.401; runtime 10.0.12, MIT with vendor notices |
| MicroCom.Runtime / Tmds.DBus.Protocol | Native interop / Linux desktop services | 0.11.6 / 0.94.1, MIT |
| Avalonia.Headless | Tests only | 12.1.3, MIT |

Rust toolchain and Python 3 are build inputs. Sparkle is the pinned third-party Swift package; there are no bundled application fonts/media (headless tests alone use Avalonia.Fonts.Inter 12.1.3, OFL-1.1 font/MIT wrapper). All Avalonia packages include the self-contained .NET runtime and exact dependency notices. X11/fontconfig/OpenGL remain Linux system libraries. No WinUI, Windows App SDK, GTK, libadwaita or JSON-GLib application dependency remains. Build-time telemetry is opted out in the entry scripts and CI. The optional icon generator uses librsvg locally; application builds use committed artwork. Consult lockfiles for all transitive packages and [LICENSE_AUDIT.md](LICENSE_AUDIT.md) before redistributing a Windows binary.

## Updater dependencies

`Cargo.lock` pins the standalone updater's ring, SHA-256, SemVer and HTTPS transport dependencies. Sparkle 2.9.6 is pinned by `Package.resolved` and an independent archive digest in `script/prepare_sparkle.sh`; its complete upstream license accompanies Rust notices. WiX 4.0.6 is a Windows build tool. Linux build-tool and AppImage runtime digests are recorded in `updates/build-tools.json`; they are build inputs, not application update channels. The generated dependency inventory records these sources. Neither updater crate depends on AVID Core.
