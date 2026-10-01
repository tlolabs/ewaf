# Dependencies

The [machine-readable inventory](../dependency-inventory.json) is generated from `Cargo.lock`, `platform/windows/EWAF/packages.lock.json` and declared native build/runtime inputs. `THIRD_PARTY_NOTICES.md` contains locked Rust license texts; Windows packages gather NuGet and .NET vendor notices. Regenerate the inventory/notices when dependencies change. The inventory records package metadata, not a legal compatibility verdict.

| Direct dependency | Purpose | Source and declared license |
| --- | --- | --- |
| Chrono | Gregorian dates | crates.io; MIT or Apache-2.0 |
| cap-std | Directory-relative filesystem access | crates.io; Apache-2.0 with LLVM exception, Apache-2.0 or MIT |
| serde / serde_json | Typed Rust/ABI serialization | crates.io; MIT or Apache-2.0 |
| unicode-normalization / unicode-general-category | Localized date search | crates.io; MIT/Apache-2.0 and Apache-2.0 respectively |
| tempfile | Rust tests only | crates.io; MIT or Apache-2.0 |
| Microsoft.WindowsAppSDK | Windows native WinUI 3 and self-contained runtime | NuGet 2.4.0; Microsoft Windows App SDK binary terms (see [license audit](LICENSE_AUDIT.md)) |
| SwiftUI / Foundation | macOS native UI and local preferences | Apple platform frameworks; platform SDK terms |
| GTK 4 / libadwaita / GLib / json-glib | Linux native UI, settings and JSON bridge | Distribution-provided dynamic libraries; LGPL/GPL-family terms vary by component/package |
| .NET 8 | Windows runtime and compiler | Microsoft/.NET; runtime redistribution notices included in package |

Rust toolchain and Python 3 are build inputs. Sparkle is the pinned third-party Swift package; there are no bundled fonts/media. Linux packages use distribution libraries; Windows portable ZIPs include runtime files with their own terms. The optional icon generator uses librsvg locally; application builds use committed artwork. Consult lockfiles for all transitive packages and [LICENSE_AUDIT.md](LICENSE_AUDIT.md) before redistributing a Windows binary.

## Updater dependencies

`Cargo.lock` pins the standalone updater's ring, SHA-256, SemVer and HTTPS transport dependencies. Sparkle 2.9.6 is pinned by `Package.resolved` and an independent archive digest in `script/prepare_sparkle.sh`; its complete upstream license accompanies Rust notices. WiX 4.0.6 is a Windows build tool. Linux build-tool and AppImage runtime digests are recorded in `updates/build-tools.json`; they are build inputs, not application update channels. The generated dependency inventory records these sources. Neither updater crate depends on AVID Core.
