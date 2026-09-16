# Cross-platform parity evidence

Status as of the migration work. **Implemented** means source exists; it is not a claim of runner, accessibility or owner acceptance. Keep this matrix current for every user-facing change.

| Capability | macOS SwiftUI | Windows WinUI 3 | Linux GTK/libadwaita |
| --- | --- | --- | --- |
| Date selection, weekday, exact/historical dates | Existing UI + 30 Swift regressions pass through Rust | Implemented, native build pending | Implemented; GTK widget validation test passes on development host |
| Chronological preview, 200 matches, search leaves plan intact | Unit and UI tests pass | Shared core + C# integration test implemented | Widget test passes |
| Full-range arithmetic, naming, thresholds | Shared Rust fixtures/tests pass | Same core; native execution pending | Same core |
| Folder picker | Existing XCTest UI workflow passes | HWND-initialized FolderPicker implemented | GtkFileDialog implemented; interactive Linux acceptance pending |
| Create/existing/content preservation/retry | Unit/workspace/UI tests pass | C# DLL integration suite implemented | GTK widget create/retry/preserved-content test passes |
| Conflict, partial results, concurrent creators | Rust and Swift tests pass | Shared core; junction/runtime checks require Windows | Shared core; Unix link/rename tests pass on development host |
| Cancellation/close | Rust atomic per-folder flag + Swift cancellation tests | CancellationToken registration implemented | Cancel action + retained GTask/application lifetime implemented |
| Large-operation confirmation | Swift unit/UI tests pass | ContentDialog implemented | AdwMessageDialog implemented |
| Default weekday | Existing AppStorage key preserved, Settings UI test passes | HKCU registry preference implemented | GSettings schema + native preferences window implemented |
| Multiple windows / restoration | Existing WindowGroup/SceneStorage preserved | New Window and last range persistence implemented | New Window, last range and geometry in GSettings |
| Menus, shortcuts, help/update links | Existing conventions preserved | Native menu and Ctrl mappings implemented | GActions/menu and Ctrl mappings implemented |
| Preview share/copy/drag | Existing ShareLink and text drag retained | Copy context menu and text drag implemented | Per-row copy button and text drag implemented |
| Destination reveal | Finder | Explorer | Default file manager |
| Appearance/scaling | Native system behavior retained | Native system behavior implemented | Native system behavior implemented |
| Accessibility | Existing identifiers and six UI tests pass; manual VoiceOver pending | Labels, focusable native controls and live status; Narrator/manual scaling pending | Native controls and named actions; Orca/manual scaling pending |
| Packaging | Universal ad-hoc package verification in progress | Self-contained ZIP/per-user install scripts, runner validation pending | Debian package/install gate, runner validation pending |
| Platform/architecture runners | macOS arm64 + Intel jobs | Windows x64 + ARM64 jobs | Ubuntu 24.04 amd64 + arm64 jobs |

## Native differences

macOS keeps its existing date form and exact-date sheet; Windows/Linux place exact date fields beside native calendar selection. This gives every platform an explicit historical-date path without relying on native calendar cutovers. macOS uses system ShareLink; Windows/Linux expose clipboard copy and text drag. Destinations are session-only everywhere. macOS restores separate scenes using the OS; Windows/Linux retain the last range rather than cloning macOS scene persistence. GTK retains last window dimensions. Appearance follows the system on every platform; EWAF did not previously have a separate appearance preference.

No platform has an unattended updater. Help/download actions open the repository. No custom telemetry or OS notifications existed in the reference; status/progress/completion are native UI. Shipping on all claimed architectures requires the CI matrix to pass. Screen-reader, high-contrast, display scaling, multiple-monitor and minimum-OS manual checks remain release acceptance gates.
