# Native architecture and compatibility

## Product scope

E.W.A.F. (Every Week a Folder) creates one folder for a selected weekday in an
inclusive date range. This is not an event calendar. It has no event CRUD,
recurrence exceptions, imports, exports, event database, notifications, or legacy
preferences. Those portions of the original generic rewrite brief do not apply.
The user confirmed this scope during the rewrite.

## Repository

- `Sources/EWAF/App`: SwiftUI lifecycle and menu commands.
- `Sources/EWAF/Views`: native window, date form, exact-date sheet, preview, settings.
- `Sources/EWAF/Stores`: main-actor observable workspace; owns async tasks and UI state.
- `Sources/EWAFCore/Models`: validated date-only values and weekly folder plans.
- `Sources/EWAFCore/Services`: actor-isolated planning, searching, and folder creation.
- `tests/EWAFCoreTests`: domain, persistence, compatibility, and regression tests.
- `tests/EWAFUITests`: Xcode-driven accessibility and end-to-end UI tests.
- `legacy-python`: runnable, unchanged baseline for manual comparison.
- Root Python entry points and tests remain available during acceptance as well.
- `script`: build/run, packaging, UI tests, and deterministic Xcode harness generation.
- `.github/workflows`: Python baseline CI and native build/test/artifact CI.

SwiftPM is the source of truth. The generated Xcode project exists for app/UI
testing and references the local `EWAFCore` package product. No third-party Swift
runtime dependency or AppKit bridge is used. macOS 14 is the minimum deployment
version; Swift 6 language mode enables strict concurrency checking.

## State and concurrency

Each SwiftUI window owns a `FolderWorkspace`; values do not leak between windows.
`@SceneStorage` restores date range and weekday with the window. Settings uses
`@AppStorage` for the default weekday (Thursday on first launch). The destination
is deliberately session-only: no persistent grant to a user's filesystem is
stored. Choose it again after restarting.

Date planning, preview filtering, and filesystem work run on a `FolderService`
actor, outside the main actor. Planning tasks are canceled when inputs change;
stale tasks cannot publish a replacement plan. Preview lists are limited to 200
rows, but creation always processes the full plan. Operations snapshot the plan
and destination and disable editing until done. Progress is delivered every 25
folders; cancellation stops before the next folder. Closing a window cancels its
operation. Multiple windows can safely target the same directory.

## Calendar correctness

`CivilDate` is a validated Gregorian year/month/day, not a timestamp. UTC is used
only as a calculation coordinate system; users' all-day folder dates never move
with DST or time-zone changes. Today's default comes from the user's current
local date. DatePicker displays date-only values; system locale controls its
presentation, while disk names always use ASCII `MM-DD-YYYY`.

Python `datetime` uses the proleptic Gregorian calendar for years 1–9999. Apple's
Foundation Calendar defaults to a Julian/Gregorian historical cutover, even with
its ISO8601 identifier. For dates before 1600, the public DateFormatter
`gregorianStartDate` API supplies the matching proleptic conversion. Weekly
addition uses Calendar day arithmetic in UTC. Tests include the October 1582
reform, Gregorian century leap rules, year 1 and year 9999. Exact-date entry
supports historical dates without relying on the native picker's cutover.

## Persistence and safe recovery

The filesystem **is** the legacy data format. There is no database migration.
Existing directories, arbitrary contents inside them, and unrelated paths stay
untouched. Python-generated fixtures establish naming/date compatibility, and
integration tests verify that existing files remain byte-for-byte identical.

The service opens the chosen destination directory, creates direct children
with atomic `mkdirat`, and inspects collisions with `fstatat` without following
symbolic links. Names can only come from validated CivilDate values. Open-directory
relative operations resist changes to the destination path during creation.
Files, symbolic links (including links to directories), and broken links stop
creation with partial counts; none are replaced or followed. This intentionally
fixes the legacy behavior that accepted directory symlinks as existing folders.

Each successful mkdir is a completed unit of work. A canceled operation or process
crash leaves already-created folders intact; repeating the same plan recognizes
them and completes the remainder. There is no destructive rollback or metadata
journal that could disagree with actual folder contents. Power-loss durability
ultimately depends on the destination filesystem; E.W.A.F. does not claim a
transaction across the entire range or durable filesystem flush per folder.

SwiftUI's file importer returns a user-selected URL. The service brackets access
with start/stopAccessingSecurityScopedResource. The design can support an App
Sandbox build with user-selected read/write access in a future App Store target.
It currently ships as a direct-distribution app.

## Accessibility and native behavior

Native controls provide labels, keyboard focus, system colors, system typography,
light/dark adaptation, and resizable layouts. Main actions also have menu commands:
Command-O chooses a destination, Command-Return creates folders, Command-period
cancels, Command-comma opens Settings, and Command-N opens another window. Preview
names can be selected through the standard list, dragged as text, or shared from
the context menu. No unrelated calendar/event features are added.
