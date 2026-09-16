# Cross-platform parity evidence

Automated evidence from the migration work; manual acceptance remains separate. **Implemented** means source exists; it is not a claim of runner, accessibility or owner acceptance. Keep this matrix current for every user-facing change.

| Capability | macOS SwiftUI | Windows WinUI 3 | Linux GTK/libadwaita |
| --- | --- | --- | --- |
| Date selection, weekday, exact/historical dates | Existing UI + native Swift regressions through Rust | Native x64/ARM64 builds and packaged date-validation UI test | GTK widget validation passes on Ubuntu amd64/arm64 |
| Chronological preview, 200 matches, search leaves plan intact | Unit and UI tests pass | Packaged C# integration tests pass | Widget test passes |
| Full-range arithmetic, naming, thresholds | Shared Rust behavioral tests | Native Rust tests pass on x64/ARM64 | Same core |
| Folder picker | Existing XCTest UI workflow passes | HWND-initialized FolderPicker implemented | GtkFileDialog implemented; interactive Linux acceptance pending |
| Create/existing/content preservation/retry | Unit/workspace/UI tests pass | Packaged C# creation/preservation/retry tests pass | GTK widget create/retry/preserved-content test passes |
| Conflict, partial results, concurrent creators | Rust and Swift tests pass | Native Rust tests pass, including Windows junction rejection | Shared core; Unix link/rename tests pass on development host |
| Cancellation/close | Rust atomic per-folder flag + Swift cancellation tests | CancellationToken registration implemented | Cancel action + retained GTask/application lifetime implemented |
| Large-operation confirmation | Swift unit/UI tests pass | ContentDialog implemented | AdwMessageDialog implemented |
| Default weekday | Existing AppStorage key preserved, Settings UI test passes | HKCU registry preference implemented | GSettings schema + native preferences window implemented |
| Multiple windows / restoration | Existing WindowGroup/SceneStorage preserved | New Window and last range persistence implemented | New Window, last range and geometry in GSettings |
| Menus, shortcuts, help/update links | Existing conventions preserved | Native menu and Ctrl mappings implemented | GActions/menu and Ctrl mappings implemented |
| Preview share/copy/drag | Existing ShareLink and text drag retained | Copy context menu and text drag implemented | Per-row copy button and text drag implemented |
| Destination reveal | Finder | Explorer | Default file manager |
| Appearance/scaling | Native system behavior retained | Native system behavior implemented | Native system behavior implemented |
| Accessibility | Existing identifiers and six UI tests pass; manual VoiceOver pending | Labels, focusable native controls and announced status changes; Narrator/manual scaling pending | Native controls and named actions; Orca/manual scaling pending |
| Packaging | Universal ad-hoc package built, launched and architecture/signature verified | Self-contained x64/ARM64 ZIPs validated; install/removal regression test in CI | Both architecture .deb packages built, installed and validated |
| Platform/architecture runners | macOS arm64 + Intel jobs | Windows x64 + ARM64 jobs | Ubuntu 24.04 amd64 + arm64 jobs |

## Verification records

- Baseline: 30 Swift tests and 12 retained Python tests passed before migration; the unchanged Python source is now preserved on the owner-authorized archive branch.
- macOS: all 31 Swift and six XCTest UI tests pass locally, with the original six workflows also passing on Apple Silicon and Intel CI. A native DatePicker accessibility-value defect was fixed without changing visible date selection. The final universal release build runs locally and passes signature, version and architecture validation.
- Rust: domain and ABI tests execute on all six architecture runners, including Unicode search differential regressions, cancellation, concurrent creators, Unix link/rename cases and Windows junction rejection.
- Linux: native GTK widget creation/search/validation/retry tests and real Debian installation checks pass on Ubuntu 24.04 amd64 and arm64.
- Windows: native compilation, packaged C# tests and WinUI accessible-control/date-validation checks pass on both x64 and ARM64. Explicit PRI/XBF staging resolves the packaged startup resource failure. Large-operation confirmation and per-user installation/removal tests pass on both x64 and ARM64.
- Full-range performance: the original Swift test was approximately 4.5 seconds; the Rust-backed Swift test is approximately 0.2 seconds locally. The Rust large-range planning/search benchmark is approximately 45 ms per iteration on this development host. These are measured comparisons, not timing guarantees.

Runner evidence: [complete six-architecture migration verification](https://github.com/tlolabs/ewaf/actions/runs/35067931349). Every final release still requires the complete matrix to pass on its own commit.

## Native differences

macOS keeps its existing date form and exact-date sheet; Windows/Linux place exact date fields beside native calendar selection. This gives every platform an explicit historical-date path without relying on native calendar cutovers. macOS uses system ShareLink; Windows/Linux expose clipboard copy and text drag. Destinations are session-only everywhere. macOS restores separate scenes using the OS; Windows/Linux retain the last range rather than cloning macOS scene persistence. GTK retains last window dimensions. Appearance follows the system on every platform; EWAF did not previously have a separate appearance preference.

No platform has an unattended updater. Help/download actions open the repository. No custom telemetry or OS notifications existed in the reference; status/progress/completion are native UI. Shipping on all claimed architectures requires the CI matrix to pass. Screen-reader, high-contrast, display scaling, multiple-monitor and minimum-OS manual checks remain release acceptance gates.

## Legacy retirement and 1.0.3

The owner authorized archiving and removing the legacy implementations on 2026-09-16. This cleanup changes no native workflow on macOS, Windows or Linux. Native regression tests remain active; the Python-derived fixtures were subsequently removed at the owner’s request. The 1.0.2 snapshot is preserved on codex/archive-legacy-1.0.2; 1.0.3 is the active application version. The owner requested an official release with the outstanding manual checks above still documented; they have not been represented as completed.

## Repository organization and application icon

Native sources now live under platform/macos, platform/windows and platform/linux; native tests live under tests/macos, tests/windows and tests/linux. Shared Rust tests remain with their crates. All legacy Python fixture data and its two consumers have been removed; native domain, boundary, persistence and filesystem regressions remain. Historical counts above describe migration evidence, not the current suite size.

The new folder/calendar icon is implemented for all three platforms from assets/icon/ewaf.svg. macOS packages and the Xcode app include an ICNS bundle icon. Windows embeds the ICO in the executable and loads it for each native window. Linux installs the application-named hicolor SVG and embeds it for source builds. No date, naming, confirmation, persistence or creation behavior changes. Local verification passes: 13 Rust domain/ABI tests, 30 Swift tests, GTK widget tests including icon resolution, native icon format checks, and universal macOS package/signature/launch checks. The six-architecture CI matrix remains pending for this change.
