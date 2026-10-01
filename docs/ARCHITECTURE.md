# Shared Rust core and native applications

The canonical behavior is [BEHAVIOR.md](BEHAVIOR.md); the baseline, stages and acceptance gates are [migration history](history/MIGRATION.md). The macOS SwiftUI interface remains the reference UI.

## Repository responsibilities

| Location | Responsibility |
| --- | --- |
| `crates/ewaf-core` | Authoritative civil dates, naming, ordering, preview, validation, threshold, portable directory operations, cancellation and errors |
| `crates/ewaf-ffi` | C ABI, versioned JSON requests, allocation ownership and operation handles |
| `bindings` | Shared C header and Swift C module map |
| `platform/macos/Sources/EWAF` | Existing macOS SwiftUI windows, views, observable UI workspace and lifecycle |
| `platform/macos/Sources/EWAFCore` | Thin Swift adapters, Codable compatibility, time-zone/display conversion, scoped URL lifetime and async scheduling |
| `platform/avalonia/EWAF` | Canonical AXAML, Fluent resources, shared workspace/commands/dialogs and C# Rust binding for Windows, Linux and internal Mac ARM64 |
| `platform/windows/EWAF.Installer` | Publisher/MSI verification and READY/COMMIT installer handoff; no presentation |
| `platform/windows/packaging`, `platform/linux/data` | OS installer scripts and desktop integration metadata |
| `tests/macos`, `tests/windows`, `tests/linux`, `crates/*/tests` | Native integration, Swift, Rust domain and ABI tests |
| `assets/icon` | Editable SVG and native icon exports shared by all three platforms |
| Archive branch | Retired Python application and original Swift history; see LEGACY_ARCHIVE.md |
| `script`, `platform/windows/packaging`, `.github` | Build/run, package validation, version checks, CI and releases |

## Boundary

Rust is an in-process static library for production macOS; Avalonia loads the same C ABI from a platform dynamic library (.dll/.so/.dylib). The C header is the only foreign ABI. JSON avoids Swift/C# representation and allocator coupling for nested models. `ewaf_request(bytes,length)` returns a NUL-terminated UTF-8 JSON allocation; callers must free it exactly once with `ewaf_string_free`. Borrowed request bytes are never retained. Date primitives use fixed-width integers and caller-owned output buffers for efficient Swift value adapters.

Responses are `{"ok":true,"value":…}` or `{"ok":false,"error":{"code":…, "message":…}}`. `info` returns ABI 1 and the workspace version. Requests are bounded to 16 MiB; malformed requests produce errors. Valid C pointers remain the caller's responsibility. Panics in request dispatch are caught at the boundary. No Rust layout or exception crosses C.

Operations: `exact` normalizes/validates start and end; `plan` accepts a nested `{start,end,weekday}` and optional `search`, `limit`, `all`; `begin` opens an operation for a validated plan and destination; `step` returns counts/status after up to 25 folders; `cancel` sets a shared atomic flag checked before every mkdir; `release` closes the operation. `all` is used by the Swift compatibility model; user previews remain bounded to 200. Handles are process-local monotonically assigned integers, never persisted. Release is idempotent. Concurrent steps for one handle serialize; unrelated operations have independent locks. Cancellation does not acquire the filesystem-operation lock. Native adapters release in finally/defer paths, including failure and cancellation.

`cap-std::fs::Dir` owns the open destination and performs relative create/no-follow metadata operations. This preserves the original mkdirat safety boundary on Unix and supplies the Windows implementation. Names originate exclusively in Rust's validated plan. Opening a user-selected destination is the only ambient filesystem entry point.

## Native responsibilities

SwiftUI and existing accessibility identifiers/layout are retained. Security-scoped resources are opened/closed around the whole Rust operation. Foundation handles current timezone and localized weekday presentation; Rust validates and calculates civil dates. Codable retains `{year,month,day}`. AppStorage `defaultWeekday` and SceneStorage `rangeStart`, `rangeEnd`, `rangeWeekday` and bundle identity are preserved. Destinations remain session-only.

The shared `Workspace` calls the preserved C# `Core` binding on Task.Run, accepts only Rust-validated plans, discards stale asynchronous responses, snapshots intent before dialogs, and registers cancellation independently of worker execution. AXAML binds to this one workspace. Native dialog/file-manager/lifecycle services sit below the presentation; there are no OS-specific views. `App` retains closing workspaces until cancellation releases their Rust directory handles. Dialogs and active operations in any window block update prompts/install handoff.

Windows continues using HKCU\Software\tlolabs\EWAF, including its existing names and test-session isolation. Linux imports the previous dconf values into an atomic JSON file under XDG_CONFIG_HOME/com.tlolabs.ewaf; migration reads the existing dconf path without retaining GTK schemas. Subsequent writes use that file. Destination access remains session-only. Internal Mac uses com.tlolabs.ewaf.avalonia-internal, with distinct preferences and no bundled update helper. Production SwiftUI preferences are untouched.

The same .NET project publishes all five Avalonia runtime identifiers. `INTERNAL_REFERENCE` is compiled for osx-arm64; the runtime additionally rejects all macOS production updater calls. Internal artifacts have their own name family and validator. The required CI reference job uploads only that family; production artifact download matching cannot include it, and release assembly rejects internal filenames. Production macOS remains separately built SwiftUI/AppKit with Sparkle.

## Maintenance

Add a shared feature to Rust first with behavioral fixtures/tests. Extend ABI requests without breaking ABI 1; incompatible changes require a new negotiated ABI. Implement native presentation and evaluate all platforms in [PARITY.md](PARITY.md). Do not move window, color, focus, geometry or settings storage into Rust. Platform-only integrations need clear native boundaries and documented equivalents. The temporary Swift snapshot was removed after the original Swift/domain/UI regressions and differential search tests passed. The complete pre-migration implementation remains in Git at 4115cc4; the runnable Python reference is archived on codex/archive-legacy-1.0.2 along with the retired Python-derived fixture data.

Primary dependency references: [Chrono civil dates](https://docs.rs/chrono/latest/chrono/struct.NaiveDate.html), [cap-std directory capabilities](https://docs.rs/cap-std/latest/cap_std/fs/struct.Dir.html), [Avalonia storage provider](https://docs.avaloniaui.net/docs/services/storage-provider), [Avalonia supported platforms](https://docs.avaloniaui.net/docs/supported-platforms).

## Distribution boundary

`crates/tlo-updater` is independently versioned and has no EWAF/AVID dependency. `crates/ewaf-update` binds it to the EWAF identity, version and compiled public trust. Native clients own UI/lifecycle/preferences. Sparkle is the macOS installer and scheduler. [UPDATING.md](UPDATING.md) records the protocol, adapters, security model, audit and qualification limits.
