# Engineering Review

## Repository assessment

EWAF is a Python 3.10+ Tkinter desktop utility. It uses `ttkbootstrap` for
the themed window and date pickers, creates date-named directories on the local
filesystem, and has no network services or persistent data store. Installation
uses `requirements.txt`; execution is direct through `ewaf.py`. The repository
initially had no tests, CI, packaging, signing, or deployment configuration.

## Baseline

- Git: clean `main` at `c90a5c0` before review.
- Syntax: source parsed successfully. Bytecode compilation was initially blocked
  by the read-only execution sandbox attempting to write `__pycache__`.
- Tests: `unittest` discovered zero tests and returned status 5.
- Dependencies: the environment had Python 3.14.7 and `ttkbootstrap` 1.20.1;
  `pip check` reported no broken requirements. PyPI lists 2.2.2 as current; the
  supported range now accepts the current major release while requiring the
  latest 1.20 patch fixes.
- UI: source inspection found a fixed-size window, modal-only status feedback,
  and unhandled filesystem failures. Interactive validation remained pending at
  the start of the review.

## Findings and remediation

| Priority | Finding / root cause | Remediation | Status |
| --- | --- | --- | --- |
| High | Importing `ewaf.py` immediately built and ran the GUI, coupling domain behavior to process lifecycle and preventing tests. | Extracted pure logic to `ewaf_core.py`; added `build_app()` and a guarded `main()`. | Complete |
| High | Filesystem failures escaped the UI and could terminate the callback after partially creating a range. | Added structured partial results, a domain error, guarded UI handling, and retry-safe reporting. | Complete |
| High | `exists()` followed by `makedirs()` introduced a race and treated any existing path as a valid directory. | Replaced it with atomic `Path.mkdir()` and explicit file-collision handling. | Complete |
| Medium | Folder-name input could escape the chosen directory if the helper were reused with untrusted names. | Validates the complete request before mutation and rejects empty, nested, absolute, and parent-relative names. | Complete |
| Medium | A very large range could start hundreds of thousands of synchronous filesystem operations without warning. | Added explicit confirmation above 250 folders and visible in-progress status. | Complete |
| Medium | Reversed ranges, empty matches, cancellation, and I/O errors had weak or misleading feedback. | Added precise messages, parented dialogs, chooser context, and persistent status text. | Complete |
| Medium | Date generation scanned every calendar day. | Advances directly to the first match, then steps in seven-day increments. | Complete |
| Medium | The optimized weekly step could overflow after processing `date.max`. | Added a terminal-range guard and regression coverage for Python's maximum date. | Complete |
| Medium | The fixed window prevented adaptation to font scaling or translated/system text sizing. | Added a minimum size, flexible grid column, and resizable layout. | Complete |
| Medium | Flatly's gray status text and teal success button had weak contrast on white. | Uses the default high-contrast label foreground and dark primary action style. | Complete |
| Medium | No automated regression coverage or CI existed. | Added domain and date-parsing regression tests plus a least-privilege GitHub Actions matrix for Python 3.10 and 3.14. | Complete |
| Low | Setup and runtime behavior were underspecified. | Documented runtime support, Linux Tk prerequisite, results, failure behavior, and test command. | Complete |

## Architecture and compatibility decisions

The domain module depends only on the Python standard library. The UI remains a
small functional composition because a larger framework would add indirection
without benefit. Folder names, inclusive range semantics, the default Thursday,
the visual theme, and the direct-run entry point remain compatible. The minimum
runtime is documented as Python 3.10 to match current `ttkbootstrap` support.

Tk widgets were visually verified on macOS, including layout, focus indication,
and scaling at the minimum window size. The macOS accessibility hierarchy exposed
the window chrome but not the nested Tk controls, so screen-reader semantics could
not be fully validated without replacing the cross-platform UI toolkit. This is a
known accessibility limitation and the only architectural risk left open.

## Final report

### Initial condition and root causes

The application ran its entire UI at module import, mixed domain and presentation
state, performed race-prone filesystem checks, did not surface partial failures,
and had no automated verification. These issues came from a single-file prototype
structure rather than from an unsuitable product design.

### Significant changes

- Extracted date generation and folder creation into an import-safe domain module.
- Made directory creation atomic, constrained it to direct children, and added
  structured created/existing/partial-failure results.
- Improved validation and feedback for invalid dates, reversed and empty ranges,
  cancellation, large operations, existing folders, and filesystem failures.
- Made the layout resizable, added persistent status, parented all dialogs, and
  improved text and primary-action contrast while preserving the Flatly theme.
- Changed weekly generation from daily scanning to direct seven-day steps. A full
  Python date-range stress test generated 521,723 entries without overflow.
- Updated `ttkbootstrap` from an installed 1.20.1 to 2.2.2 and constrained supported
  installs to `>=1.20.4,<3`; no other runtime dependency was added.
- Added 12 unit tests for date parsing, inclusive ranges, empty/reversed/boundary
  ranges, invalid weekdays, atomic creation, existing directories, file collisions,
  traversal rejection, partial progress, and missing destinations.
- Added GitHub Actions checks on Python 3.10 and 3.14 with read-only repository
  permissions, and expanded setup, runtime, Linux, failure, and test documentation.

### Validation results

- `python -m unittest discover -s tests -v`: 12 passed.
- `ruff check .`: passed with no findings.
- `ruff format --check .`: all 6 Python files formatted.
- `python -m compileall -q ewaf.py ewaf_core.py tests`: passed.
- `pip check`: no broken requirements with `ttkbootstrap` 2.2.2.
- Maximum date-range stress test: passed (521,723 weekly dates).
- `git diff --check`: passed.
- macOS interactive smoke test: app launched on Python 3.14.7, rendered at its
  460x250 default size, showed keyboard focus and working controls, and cleanly
  constructed and closed against `ttkbootstrap` 2.2.2.

### Compatibility and remaining limitations

Folder naming, inclusive ranges, Thursday default, visual theme, direct launch,
and retry behavior remain compatible. Python 3.10 is the documented minimum. The
new GitHub Actions workflow was reviewed locally but cannot be executed until it
is pushed to GitHub. Packaging, signing, and deployment remain out of scope for
this source-run utility. No credentials, external services, or hardware are used.

The nested Tk controls were not exposed through the macOS accessibility hierarchy
during inspection, so VoiceOver semantics remain unverified. Resolving that fully
would require replacing the cross-platform Tk UI; visual focus, scalable layout,
contrast, and keyboard traversal were preserved or improved within the toolkit.

## Pre-SwiftUI baseline (2026-09-15)

Reviewed all source, tests, documentation, working-tree changes, and Git history.
All 12 Python tests passed again and the ttkbootstrap window successfully built,
rendered, and closed. GitHub has no open or closed issues at this checkpoint.
This commit preserves the legitimate prior engineering review before the native
rewrite. The product is a weekly folder generator, not an event calendar; its
persistent user data consists solely of existing folders and their contents.
There are no saved preferences, database, import/export formats, custom icons,
bundle identifier, or automatic updater to migrate. The Python implementation
will remain runnable in legacy-python/ pending explicit user acceptance.
