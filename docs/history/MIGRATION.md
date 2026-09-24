# Rust and native UI migration

## Audit baseline — 2026-09-15

Inspected every tracked application source, tests, fixtures, scripts, generated Xcode project, workflows and documentation at commit `4115cc40716296e3d0de0e4a58c85de5c3e59450`. The working tree was clean. README is supplementary; source and regression tests define the baseline.

The Swift 6 package has EWAF (SwiftUI application) and EWAFCore (Foundation domain models and actor service). Xcode is a generated UI-test harness consuming EWAFCore. Six views, an observable per-window workspace, two domain model files and one service implement the application. No third-party Swift dependency exists.

Dates are proleptic Gregorian years 1–9999, inclusive and matched to one weekday. Thursday is the initial preference. Names are ASCII MM-DD-YYYY. Plans advance weekly and remain chronological across years. Exact entry trims surrounding whitespace, rejects malformed dates and reversed ranges, and supports historical dates that the system picker cannot represent faithfully. Preview returns the first 200 search matches; searching never changes creation. Swift currently uses localizedStandardContains, including Unicode width-insensitive matching; migration must cover that behavior.

Creation confirms above 250 folders, snapshots the plan and destination, disables editing and executes off the main actor. The actor opens the destination once, uses mkdirat and no-follow fstatat, reports every 25 folders and stops at the first failure or cancellation. Directories count as existing and retain contents; ordinary files and links stop the operation. Retry recognizes completed directories. Concurrent windows are supported. Security-scoped URL lifetime stays on macOS. Destination access is session-only. No destructive rollback or journal exists.

Each window restores rangeStart, rangeEnd and rangeWeekday through SceneStorage. defaultWeekday uses AppStorage; corrupt values fall back to Thursday. Preserve these keys and com.tlolabs.ewaf. WindowGroup supplies multiple windows/restoration, default 780×540 and minimum 640×480. Menus supply destination (Command-O), creation (Command-Return), cancellation (Command-period), Settings (Command-comma), new window (Command-N), help and authenticated GitHub update downloads. Preview supports sharing and dragging folder names as text; Finder opens the destination. File selection is a SwiftUI native importer. Native controls supply system appearance, scale and accessibility labels/identifiers. No custom telemetry, notifications, updater, database, import/export or stored destination permission exists; --telemetry merely streams process logs.

Tests cover dates, timezone adapters, 112 Python fixtures, maximum-range count, search, threshold, Codable validation, filesystem preservation/conflicts/concurrency/cancel/retry, workspace transitions and six XCTest UI workflows. Python root and legacy-python copies match and are retained. Python uses datetime, synchronous creation, a Tk UI, no settings, and accepts directory symlinks; the safer Swift rule is authoritative. Python strftime padding can vary by host for ancient years; shared fixtures use four-digit years.

Existing scripts build/package a SwiftPM app, ad-hoc sign by default, optionally Developer ID sign/notarize, generate the Xcode harness and run UI tests. CI runs Python 3.10/3.14, macOS unit/UI tests and universal packaging; tags produce draft releases. There are no Windows/Linux apps or tests at baseline. Signing secrets are optional and never tracked. CODE_REVIEW_PLAN.md describes the earlier Python review and is historical.

## Architecture and ownership

Rust owns validated civil dates, Gregorian arithmetic, names, ordering, plans, filtering, confirmation threshold, directory conflict classification, partial result/error models, bounded creation steps and cancellation state. Chrono supplies calendar primitives; cap-std holds an open directory capability so portable creation does not regress the existing destination-relative behavior. The C ABI exposes date primitives and versioned JSON requests with Rust-owned response allocation/free. Native callers never provide arbitrary child names to filesystem operations.

Swift remains the macOS UI. WinUI 3 supplies Windows UI; GTK 4/libadwaita supplies Linux UI. Each native layer owns UI thread scheduling, menus, controls, dialogs, clipboard/drag/share/file-manager integration, lifecycle, accessibility, local today/timezone conversion and native settings. No cross-platform UI framework. Core errors expose stable codes and useful fallback text; platforms may localize presentation. UI and preference state never enter Rust.

ATIV and EnCAP sibling manifests/workflows were inspected: both use macOS 13, WinUI targeting Windows 10 1809 and native architecture-specific packaging. ATIV uses Ubuntu 24.04 with GTK 4.10/libadwaita 1.4 and both Linux architectures. EWAF retains its existing macOS 14 requirement; Windows targets 1809 and Linux uses Ubuntu 24.04. No existing OS support is removed.

## Stages and acceptance

1. Record this audit and execute existing tests before replacing logic.
2. Implement/test Rust with existing fixtures, boundaries and filesystem adversarial cases.
3. Add/test C bindings; migrate Swift models and service incrementally while preserving views. Keep a reference snapshot until UI regression checks pass.
4. Implement WinUI and GTK workflows against the same ABI.
5. Add architecture matrices, dependency locks, packaging validation and unified-version development/tag release gates.
6. Audit parity, document actual verification and remove only verified obsolete active code.

Acceptance requires passing Rust lint/tests, C boundary tests, existing Swift/workspace/UI tests, native Windows/Linux builds and integration tests, safe packaged launches, correct architecture/version, and manual keyboard/screen-reader/appearance/scaling/multiple-display checks. Source implementation alone is not verified parity. Missing toolchains or credentials must be reported explicitly. No platform artifact may be released after a failed required check. Native UI changes must be evaluated on all three platforms and significant compromises or compatibility breaks require owner approval.

## Verification milestones

The baseline passed 30 Swift tests and 12 Python tests. After migrating models and filesystem orchestration, all original Swift tests and six UI workflows passed through Rust. An additional Foundation/Rust Unicode search differential test and accessible date-picker value assertions were added and passed. The temporary non-built Swift snapshot was then removed; commit 4115cc4 preserves the complete original implementation. Python files were preserved unchanged through migration and are now retained on the archive branch. Native GTK widget tests pass on the development host and both Ubuntu architectures, including installed Debian package checks. Windows Rust tests (including junction rejection), native compilation and packaged C# integration passed; packaged WinUI launch and accessible date validation now pass on both Windows architectures after explicit native resource staging. See PARITY.md and CI for final status.

## Owner-authorized retirement — 2026-09-16

After the complete native matrix passed, the owner authorized removal of the legacy implementations. Version 1.0.2 archives the full pre-cleanup repository on codex/archive-legacy-1.0.2. Version 1.0.3 removes Python applications, Python application tests/dependencies/CI and the obsolete Python review plan. Native source, build tooling and migration documentation remain active. The owner subsequently requested removal of all Python-derived fixture data and its comparison tests. See [archive and recovery](../LEGACY_ARCHIVE.md).
