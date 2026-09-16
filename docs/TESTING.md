# Testing and acceptance

Run from the repository root. Rust dependencies are locked; the native bindings must be built before Swift tests.

```sh
cargo fmt --all -- --check
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo test --workspace --locked
python3 script/check_versions.py
./script/build_core.sh
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer swift test
./script/test_ui.sh
```

On Linux install the development dependencies listed in README, then run `xvfb-run -a dbus-run-session -- ./script/test_linux.sh`. This compiles with warnings as errors, checks the linked core, creates real GTK widgets and verifies date validation, searchable preview, creation, preservation and idempotent retry. GSettings uses an in-memory backend in tests. Folder-dialog and assistive-technology acceptance require interactive Linux testing.

On Windows run `./script/package_windows.ps1 -Architecture x64` (or ARM64), add the resulting `dist/windows-<architecture>` folder to PATH, and run `dotnet run --project platform/windows/Tests/Tests.csproj -c Release`. The test loads the actual packaged Rust DLL and exercises preview, creation, preservation, repeat operations and cancellation. `powershell -NoProfile -File script/test_windows_ui.ps1 -Executable dist/windows-x64/EWAF.exe` verifies packaged WinUI launch, accessible controls, invalid-date handling and large-operation confirmation. `./script/test_windows_install.ps1 -PackageDirectory dist/windows-x64` verifies per-user installation/removal and preservation of both empty and populated generated folders. Narrator checks remain separate.

Rust tests include 112 shared Python fixtures, all seven weekdays over the full supported date range, Gregorian boundaries, leap/century rules, malformed/unsafe input, ordering, search/confirmation, retained contents, partial conflict recovery, destination failures, concurrent creators, cancellation and open-directory rename resistance. ABI tests cover malformed envelopes, date primitives, response allocation/free and invalid handles. A Swift differential test also compares representative Unicode searches with the original Foundation localized search. Swift keeps its original date/timezone/Codable and workspace tests plus the existing six accessibility-driven UI workflows. The full-range Swift test provides a useful broad performance regression signal (baseline ~4.5 s, Rust-backed ~0.2 s locally; timings vary).

`script/generate_xcode_project.py` generates the Xcode UI-test harness. Commit the generated project; CI rejects drift. UI tests require a logged-in graphical session and Xcode testing access. Test directories are uniquely named and only their fixtures are removed.

## Manual acceptance on each OS and architecture

- Confirm native layout, keyboard focus/order, minimum window size, resizing and multiple windows.
- Check single-day matching/nonmatching ranges, reversed dates, invalid leap days and October 1582 Sundays.
- Confirm exact years 0001 and 9999 and chronological order across years.
- Create into a temporary destination with existing user contents; repeat and verify bytes remain unchanged.
- Exercise a file, broken link, directory link and (Windows) junction collision, then recover and retry.
- Confirm above 250 folders, cancel during creation, close during creation, disconnect a mounted drive and retry.
- Exercise folder picker cancellation, open destination, copy/share and drag folder names.
- Check VoiceOver/Narrator/Orca, high contrast, keyboard-only use, dark/light system appearance, high-density displays, larger text and multiple monitors.
- Validate package installation/removal and first launch on the minimum OS. Removing the app must preserve generated directories and preferences.

Do not equate widget labels with complete screen-reader acceptance. Record actual runner/manual results in PARITY.md. Missing credentials must not block ad-hoc/unsigned development builds; signing acceptance is separate from functionality.

The retired Python test suite is preserved on the archive branch; it is no longer an active CI dependency. The 112 shared reference cases remain in `tests/EWAFCoreTests/Fixtures/calendar-dates.json` and run directly in Rust and Swift. See [legacy archive](LEGACY_ARCHIVE.md).
