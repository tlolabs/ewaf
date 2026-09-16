# Shared Rust core and native applications

The canonical behavior is [BEHAVIOR.md](BEHAVIOR.md); the baseline, stages and acceptance gates are [MIGRATION.md](MIGRATION.md). The macOS SwiftUI interface remains the reference UI.

## Repository responsibilities

| Location | Responsibility |
| --- | --- |
| `crates/ewaf-core` | Authoritative civil dates, naming, ordering, preview, validation, threshold, portable directory operations, cancellation and errors |
| `crates/ewaf-ffi` | C ABI, versioned JSON requests, allocation ownership and operation handles |
| `bindings` | Shared C header and Swift C module map |
| `Sources/EWAF` | Existing macOS SwiftUI windows, views, observable UI workspace and lifecycle |
| `Sources/EWAFCore` | Thin Swift adapters, Codable compatibility, time-zone/display conversion, scoped URL lifetime and async scheduling |
| `platform/windows/EWAF` | WinUI 3 controls, Windows picker, clipboard/drag, registry preferences and native executor |
| `platform/linux` | GTK 4/libadwaita widgets, GSettings, folder picker, clipboard/drag and GLib worker tasks |
| `tests/EWAFCoreTests/Fixtures` | Shared 112-case Python-derived golden fixture read directly by Swift and Rust |
| `tests`, `crates/*/tests`, `platform/*/tests` | Swift/domain/ABI/native integration tests (Windows uses `Tests`) |
| `reference-swift` | Non-built pre-migration Swift snapshot for differential verification; never production dependencies |
| `legacy-python`, root Python files | Retained runnable Python reference; unchanged |
| `script`, `packaging`, `.github` | Build/run, package validation, version checks, CI and releases |

## Boundary

Rust is an in-process static library on macOS/Linux and a DLL on Windows. The C header is the only foreign ABI. JSON avoids Swift/C#/C representation and allocator coupling for nested models. `ewaf_request(bytes,length)` returns a NUL-terminated UTF-8 JSON allocation; callers must free it exactly once with `ewaf_string_free`. Borrowed request bytes are never retained. Date primitives use fixed-width integers and caller-owned output buffers for efficient Swift value adapters.

Responses are `{"ok":true,"value":…}` or `{"ok":false,"error":{"code":…, "message":…}}`. `info` returns ABI 1 and the workspace version. Requests are bounded to 16 MiB; malformed requests produce errors. Valid C pointers remain the caller's responsibility. Panics in request dispatch are caught at the boundary. No Rust layout or exception crosses C.

Operations: `exact` normalizes/validates start and end; `plan` accepts a nested `{start,end,weekday}` and optional `search`, `limit`, `all`; `begin` opens an operation for a validated plan and destination; `step` returns counts/status after up to 25 folders; `cancel` sets a shared atomic flag checked before every mkdir; `release` closes the operation. `all` is used by the Swift compatibility model; user previews remain bounded to 200. Handles are process-local monotonically assigned integers, never persisted. Release is idempotent. Concurrent steps for one handle serialize; unrelated operations have independent locks. Cancellation does not acquire the filesystem-operation lock. Native adapters release in finally/defer paths, including failure and cancellation.

`cap-std::fs::Dir` owns the open destination and performs relative create/no-follow metadata operations. This preserves the original mkdirat safety boundary on Unix and supplies the Windows implementation. Names originate exclusively in Rust's validated plan. Opening a user-selected destination is the only ambient filesystem entry point.

## Native responsibilities

SwiftUI and existing accessibility identifiers/layout are retained. Security-scoped resources are opened/closed around the whole Rust operation. Foundation handles current timezone and localized weekday presentation; Rust validates and calculates civil dates. Codable retains `{year,month,day}`. AppStorage `defaultWeekday` and SceneStorage `rangeStart`, `rangeEnd`, `rangeWeekday` and bundle identity are preserved. Destinations remain session-only.

WinUI schedules Rust on Task.Run, marshals progress to the UI context, and registers cancellation independently of worker execution. Its unpackaged per-user settings use HKCU\Software\tlolabs\EWAF; ApplicationData.Current requires a packaged identity and is not used. FolderPicker is initialized with the native window handle. GTK uses GTask, retained window/application references and GSettings. It keeps the app alive until a closing window's operation has canceled and released its directory handle. Settings belong to the native layers, not Rust.

## Maintenance

Add a shared feature to Rust first with behavioral fixtures/tests. Extend ABI requests without breaking ABI 1; incompatible changes require a new negotiated ABI. Implement native presentation and evaluate all platforms in [PARITY.md](PARITY.md). Do not move window, color, focus, geometry or settings storage into Rust. Platform-only integrations need clear native boundaries and documented equivalents. Keep the reference snapshots until regression/acceptance gates pass, then remove obsolete copies rather than introducing a second authority.

Primary dependency references: [Chrono civil dates](https://docs.rs/chrono/latest/chrono/struct.NaiveDate.html), [cap-std directory capabilities](https://docs.rs/cap-std/latest/cap_std/fs/struct.Dir.html), [GTK FileDialog](https://docs.gtk.org/gtk4/class.FileDialog.html), [WinRT picker window initialization](https://learn.microsoft.com/en-us/windows/apps/develop/ui/display-ui-objects).
