# EWAF behavior specification

EWAF (Every Week a Folder) creates one direct child directory for each occurrence of one selected weekday in an inclusive date range. This document is the shared product contract. Tests and the parity matrix identify its verification status.

## Dates and preview

- Use proleptic Gregorian civil dates from year 1 through 9999. Leap years are divisible by 4 except centuries not divisible by 400. Time zones and DST never change a selected folder date. Initial today is the local date supplied by the platform.
- Exact input accepts ASCII `MM-DD-YYYY` with four year digits; surrounding whitespace is trimmed by the exact-entry operation. Folder-name parsing itself is strict. Reject invalid dates, malformed input and reversed ranges without filesystem changes.
- The initial weekday is Thursday. Persisted/native interface numbering is Sunday=1 through Saturday=7; UI order is Monday through Sunday. An invalid saved preference falls back to Thursday. Default weekday applies to new windows.
- Select the first matching weekday at or after the start, advance by seven days, include the end if it matches, and stop without overflow. Zero matches is valid and disables creation.
- Folder names are zero-padded ASCII `MM-DD-YYYY`. Preview order is chronological, including across year boundaries. Searching filters names, supports full-width numeric input, and shows at most 200 matches. Searching never filters the creation plan.
- More than 250 planned folders requires confirmation before destination selection/creation. Exactly 250 does not.

## Creation, errors and recovery

The user chooses an existing destination. Creation uses a snapshot of the complete validated range, weekday and destination. Date/destination controls are disabled while creating. Planning/filtering and filesystem steps run away from the UI thread.

Open the destination once and create only validated direct children relative to that directory capability. An existing directory is retained and counted; its contents are never examined or modified. Existing files, symbolic links (including directory and broken links), and Windows junctions must not be followed or replaced. Stop at the first conflict/error, report its folder name and completed counts, and offer useful corrective guidance. Do not pre-scan with a race-prone check-then-create sequence.

Progress reports after at most 25 processed folders. Cancellation is checked before each folder; it cannot interrupt an OS call already in progress. Window closure requests cancellation. Created directories remain in place on cancellation/failure/crash. There is no rollback, database, journal or claim of an atomic multi-folder transaction. Repeating the same range safely recognizes prior directories and completes the rest. Concurrent windows/creators must not overwrite data.

Errors distinguish invalid input, unavailable destination, collision, permission denial/read-only volume, insufficient storage/quota and other I/O failures. Native UIs display completion, cancellation or stopped-operation summaries with separate created/existing counts. Permission failures are recoverable by choosing another destination or correcting permissions and retrying.

## Native state and workflows

Folder selection may precede creation or be requested by Create Folders. Canceling the chooser creates nothing. Destination access lasts only for the session/operation; it is not silently persisted across restarts. Open Folder invokes Finder, Explorer or the Linux file manager. Preview names support native sharing or copying and text drag-and-drop.

Each window has independent working state. Range restoration, geometry, default-weekday persistence, local date/weekday presentation, system light/dark appearance, display scaling, menus, shortcuts and accessibility remain native. No event calendar, import/export format, background network service, unattended updater or custom telemetry existed to migrate.

## Change rule

Any intentional user-facing change requires an explicit macOS/Windows/Linux evaluation in the PR and parity matrix. Implement equivalent intent using native conventions. A significant parity compromise or compatibility break requires owner approval. Preserve `com.tlolabs.ewaf` and macOS preference keys. Never describe an untested platform as verified.
