# Testing and manual acceptance

## Automated commands

```sh
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
swift test
./script/test_ui.sh
.venv/bin/python -m unittest discover -s tests -v
```

Xcode is required for XCTest; a Command Line Tools-only toolchain can build the
app but cannot resolve XCTest. Set `DEVELOPER_DIR` rather than changing the
machine-wide selected toolchain. UI tests must run in an unlocked graphical
session with Xcode's testing/automation access authorized. They create uniquely
named temporary directories and delete only those fixtures on completion.

The deterministic `script/generate_xcode_project.py` refreshes the Xcode UI-test
harness when app or UI-test source files change. Commit the generated project.
CI checks that regeneration produces no diff.

## Coverage

- Inclusive ranges, first matching weekday, empty/reversed ranges, all weekdays.
- Leap days and Gregorian century rules, DST transitions, fractional offsets,
  date-only behavior across time zones, year boundaries and chronological order.
- Historical Gregorian dates, years 1–9999, malformed names and decoding.
- Python-generated fixtures and full-range weekly count parity.
- Preview search, 200-row bound, and the >250-folder confirmation threshold.
- New/existing directories, unchanged user files, file and symbolic-link
  collisions, missing destinations, partial recovery, concurrent creators,
  cancellation, and retry.
- UI accessibility controls, search, native folder selection, folder creation,
  retry with user data, and Settings keyboard access.

The legacy app has no events, event recurrence exceptions, import/export formats,
or stored preferences. Event CRUD and database migration tests would test
invented functionality; compatibility tests exercise the real folder format.

## Owner acceptance checklist

- Launch the development app; verify name, layout, resizing and window controls.
- Try a one-day matching and nonmatching range, reversed dates, and a leap day.
- Use Enter Exact Dates for `10-01-1582` through `10-31-1582`, selecting Sunday;
  confirm all five Sundays, including October 10, appear.
- Use a temporary destination containing a matching folder and some user files.
  Create folders, repeat, and confirm original contents are unchanged.
- Create a colliding ordinary file or symlink; verify partial results and safe
  recovery after moving the collision away.
- Confirm a range above 250 folders; cancel during creation and retry.
- Use all controls by keyboard; check Command-O, Command-Return, Command-period,
  Command-comma and Command-N. Check VoiceOver reading order and announcements.
- Check light/dark mode, Increase Contrast, Reduce Motion, larger text settings,
  Retina rendering, multiple displays, and restored windows.
- Test on the oldest supported macOS and an Intel Mac before public release.

Automated access to labels is evidence of accessibility semantics, but does not
replace a human VoiceOver and multi-display acceptance pass. Signing and
notarization are separate from functional acceptance.
