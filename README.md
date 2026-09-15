# E.W.A.F. — Every Week a Folder

A native SwiftUI Mac app that creates one folder for every selected weekday in
an inclusive date range. Folder names use `MM-DD-YYYY`.

## Use

1. Choose start and end dates and a weekday (Thursday by default).
2. Review the chronological folder preview; search for a particular date.
3. Click **Create Folders** (Command-Return), then choose a destination. You can
   also choose the destination first with Command-O.
4. E.W.A.F. reports new and existing folders. Existing contents stay intact.

**Enter Exact Dates…** accepts `MM-DD-YYYY` values, including historical dates.
More than 250 folders requires confirmation. Large operations run in the
background and can be canceled with Command-period. If an operation stops, fix
the reported problem and repeat the same range to safely finish it.

**Settings** (Command-comma) sets the default weekday for new windows.
The system controls appearance, date presentation, and window restoration.

## Build and run

Requires macOS 14 or later and a Swift 6 toolchain. Xcode is required for tests.
There are no Python or third-party runtime dependencies in the native app.

```sh
./script/build_and_run.sh --verify
```

The app is staged at `dist/EWAF.app`. The repository also supplies a Codex Run
button. Optional script modes: `--debug`, `--logs`, `--telemetry`, `--verify`.

```sh
# If xcode-select currently points at Command Line Tools:
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
swift test
./script/test_ui.sh
# Release build for both Apple silicon and Intel:
UNIVERSAL=1 ./script/package.sh
```

## Downloads and distribution

Every successful main-branch [Native macOS workflow](https://github.com/tlolabs/ewaf/actions/workflows/swift.yml)
provides a zipped universal development app. The repository is private, so
GitHub sign-in and repository access are required. Development builds are ad-hoc
signed. Developer ID signing and notarization require the owner's credentials.
Tagged builds create draft GitHub releases for review.

See [release instructions](docs/RELEASING.md), [architecture and persistence](docs/ARCHITECTURE.md),
and [testing and acceptance](docs/TESTING.md).

## Python baseline retained for acceptance

The original implementation is preserved in `legacy-python/`; root Python entry
points remain available too. It will not be removed or archived until the owner
explicitly accepts the native replacement.

```sh
python3 -m pip install -r legacy-python/requirements.txt
python3 legacy-python/ewaf.py
PYTHONPATH=legacy-python python3 -m unittest discover -s legacy-python/tests -v
```
