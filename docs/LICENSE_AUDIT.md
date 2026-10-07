# Current Qt 6 dependency audit

The migration completely replaces Avalonia/.NET and the historical WinUI/GTK presentations with a native Qt 6 Widgets presentation layer (`platform/qt/`). All .NET runtimes, NuGet packages, Ms-PL Silverlight-derived code, and the previous narrow linking permission (`ADDITIONAL_PERMISSION.md`) are eliminated from the active tree.

Qt 6 modules used:
- `Qt6::Core`
- `Qt6::Gui`
- `Qt6::Widgets`
- `Qt6::Concurrent`
- `Qt6::Test` (test harness only)

Qt 6 is licensed under the GNU Lesser General Public License, version 3 (LGPL-3.0-only), and the GNU General Public License, version 3 (GPL-3.0-only). Under GNU LGPLv3 §3, combining an Application with an LGPL-covered Library by dynamic linking is explicitly permitted, and EWAF's root code license is GNU GPL-3.0-or-later. Therefore:
1. Dynamic linking of Qt 6 shared libraries with GPL-3.0-or-later EWAF binaries is fully license-compatible.
2. No additional permission or license exception is required.
3. Complete LGPL-3.0 license text is provided in `licenses/qt/LGPL-3.0.txt` and incorporated into `THIRD_PARTY_NOTICES.md`.
4. No proprietary frameworks, telemetry runtimes, or non-free assets are bundled.

The Rust core (`crates/ewaf-core`, `crates/ewaf-ffi`, `crates/tlo-updater`) continues to use MIT / Apache-2.0 / BSD permissively licensed crates. Production macOS continues to use native Apple SwiftUI/AppKit frameworks under standard platform SDK terms.

---

The following sections record historical evidence for the retired presentations.

# Historical Avalonia dependency audit (2026-10-01)

The intermediate migration to Avalonia 12.1.3 was audited on 2026-10-01. Avalonia carried Silverlight-derived components under Ms-PL in Avalonia.Controls. A narrow §7 exception was granted during that period in `ADDITIONAL_PERMISSION.md`. With the complete rip-and-replace migration to Qt 6, that exception and all Avalonia/.NET components have been removed.

# Historical WinUI distribution audit (2026-09-24)

Assessed 2026-09-24 for the initial WinUI 3 distribution. Original EWAF code was GPL-3.0-or-later. WinUI 3 bundled proprietary Windows App SDK payloads that raised distribution questions. That architecture was retired in favor of shared presentation.
