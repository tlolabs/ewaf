# Qt 6 migration qualification

Rip-and-replace migration from Avalonia/.NET to Qt 6 Widgets.

## Architecture and scope

- **Production macOS**: Native SwiftUI/AppKit remains authoritative for production Mac, consuming the unchanged Rust core via C ABI.
- **Windows and Linux**: One shared Qt 6 Widgets presentation layer (`platform/qt/`), C++17, CMake, consuming the unchanged Rust core via C ABI.
- **Internal macOS ARM64 reference**: Built from the shared Qt 6 presentation for development, parity testing, and debugging. Isolated under `com.tlolabs.ewaf.qt-internal` with updates permanently disabled.
- **Authoritative Rust Core**: `crates/ewaf-core` and `crates/ewaf-ffi` remain the sole authority for business logic, Gregorian date math, validation, folder creation, conflict handling, and update mechanics. No business logic is replicated in C++.
- **Native Windows installer verifier**: Standalone C++ binary (`platform/windows/installer/ewaf-installer.cpp`) replacing the retired C# `EWAF.Installer`, using native Win32, WinTrust, and MSI APIs.
- **Total removal of Avalonia & .NET**: All Avalonia projects, .cs source, .csproj, XAML, packages.lock.json, global.json, licenses, and .NET dependencies were completely removed from the active tree. Git history is the archive.

## Technology standard

- Qt 6 (Core, Gui, Widgets, Concurrent, Test)
- C++17
- CMake 3.20+
- Unchanged Rust core (`libewaf_ffi`)
- Dynamic linking under GNU LGPLv3 (compatible with EWAF's GPL-3.0-or-later license)

## Feature parity inventory

| Capability | Replacement / preserved behavior | Evidence and status |
| --- | --- | --- |
| Date entry & validation | `QDateEdit` with calendar popups; exact range validation via Rust core | Unit and integration tests pass |
| Localized weekdays | Qt localized weekday combo box mapping to Rust `defaultWeekday` | Preserved and tested |
| Search & filtering | Search `QLineEdit` filtering preview list; stale async results discarded | Headless search and plan tests pass |
| Preview & plan | `QListWidget` with 200-row preview bound, plan count reporting | Verified in headless test harness |
| Destination chooser | Native `QFileDialog::getExistingDirectory`; session-only destination | Verified in UI tests |
| Folder creation | Background `CreationWorker` (`QThread`), conflict detection, retry | Creation and retry tests pass |
| Cancellation & cleanup | Atomic cancel flag in worker thread; file handles released before close | Worker cancellation test passes |
| Large-operation warning | Modal warning when folder count exceeds threshold; Cancel is default button | Dialog modal guard tests pass |
| Preferences storage | Windows Registry HKCU (`Software\tlolabs\EWAF`), Linux dconf import + JSON (`~/.config/com.tlolabs.ewaf/preferences.json`), internal Mac JSON | Persistence and migration tests pass |
| Multi-window support | Independent `Workspace` per window; window dimensions & last range retained | Multi-window tests pass |
| Menus & keyboard shortcuts | File/Edit/Help menus, Ctrl+N, O, Enter, W, F, C, , and macOS Command equivalents | Shortcut and action bindings verified |
| Clipboard & drag-and-drop | Copy preview lines to clipboard; drag folder names to external targets | Clipboard and drag handlers tested |
| Update integration | Windows MSI READY/COMMIT handshake; Linux AppImage replacement; disabled on internal Mac | Update coordinator tests pass |
| Accessibility | Accessible names, automation IDs (`accessibleDescription`), polite status, keyboard focus | WCAG 2.2 AA target; tested |
| CLI diagnostics | `--core-smoke` (ABI prefix) and `--ui-smoke` (window startup check) | Verified in build and packaging |

## Test suite and verification

The dedicated test suite `tests/qt/test_ewaf_qt.cpp` executes 12 shared presentation scenarios:
1. Exact Core ABI and summary integration
2. Workspace initial validation and plan calculation
3. Search filtering and plan preservation
4. Real filesystem folder creation and retry
5. Worker thread cancellation and handle release
6. Large operation confirmation dialog guard
7. Cross-platform preferences persistence and Linux dconf migration
8. Multi-window independence and lifecycle
9. Main window accessible controls and automation IDs
10. Clipboard context actions
11. Update coordinator and platform guard verification
12. CLI `--core-smoke` and `--ui-smoke` validation
