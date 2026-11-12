# EWAF follow-up verification and version 1.0.6

Date: 2026-10-30, America/Los_Angeles. Baseline: `3a3e52484fa55096870bf762418ca62d23f6c3ad`, branch `main`, working directory `/Users/tlothian/Documents/Projects/ewaf`.

This pass continued the existing fixes, corrected two additional defects and a version-diagnostic gap, and increased the application version from 1.0.5 to 1.0.6. It preserved the previous work and reports. No commit, tag, push, CI dispatch, or publication was performed.

## Starting state and evidence

The actual checkout contained the [initial report](2026-10-30-audit-and-remediation.md), [handoff](2026-10-30-handoff.md), [independent report](2026-10-30-independent-verification.md), and [earlier patch](2026-10-30-patch.diff). The pasted October 29 transcript describes another checkout; those reports were absent here and were not treated as local evidence.

Eight tracked files were already modified: `docs/PARITY.md`, `platform/qt/include/mainwindow.h`, `platform/qt/include/workspace.h`, `platform/qt/src/mainwindow.cpp`, `platform/qt/src/preferences.cpp`, `platform/qt/src/workspace.cpp`, `platform/windows/installer/ewaf-installer.cpp`, and `tests/qt/test_ewaf_qt.cpp`. Seven files were already untracked: the four October 30 audit artifacts above and `github-actions-audit.md`, `github-actions-inventory.csv`, and `github-actions-optimization.md`. All seven untracked files remain byte-identical to entry. Existing source edits were retained and extended only where described below.

The [snapshot manifest](2026-10-30-follow-up-1.0.6-snapshot.json) records entry hashes, final hashes for all tracked source/build/test/configuration inputs, evidence-log hashes, and local package hashes. The [complete source patch](2026-10-30-follow-up-1.0.6.patch) is against the baseline commit and includes retained fixes plus this pass, the new regression test, parity/changelog changes, and release notes. It excludes generated binaries, build stamps, credentials, and audit artifacts. Apply it to an isolated checkout of the baseline, not over this modified tree. The original entry diff and full execution logs remain under the ignored `.local-reports/followup-2026-10-30/` directory.

Host: macOS 27.2 / Darwin 27.2.0 ARM64; Xcode 27.2; Swift 6.4.2; pinned Rust/Cargo 1.98.1; Qt 6.12.0; CMake 4.4.5; Python 3.15.2. These are observed for this pass; historical environment and test-count claims were not reused.

## Additional findings and fixes

### FUP-01 — Picker loses a valid whitespace-wrapped date

- Severity: medium. Confidence: high; reproduced before correction.
- Trigger: enter ` 01-02-0001 ` in a Qt date field, then open its calendar picker.
- Expected: the picker selects January 2, year 1, as accepted by Rust exact-date validation. Actual: raw `QDate::fromString` rejects the surrounding spaces and leaves the calendar on its default date. Accepting that picker can replace the intended date with today.
- Root cause and fix: `MainWindow::pickDate` now calls Rust `exact` with the field as both endpoints and converts only the canonical result to `QDate`. Invalid input retains the existing correction workflow. Date parsing and normalization remain authoritative in Rust.
- Source: `platform/qt/src/mainwindow.cpp:446`; regression in `tests/qt/test_ewaf_qt.cpp` checks selected historical date and canonical text after accepting the dialog.
- Evidence: `QT_QPA_PLATFORM=offscreen ./script/test_qt.sh` exited 1 before the fix with `FAILED: Picker retains whitespace-trimmed historical date`. Afterward both offscreen and native Cocoa suites exited 0 with 22 scenarios. See `qt-calendar-before.log`, `qt-offscreen-after.log`, and `qt-cocoa-after.log` in the evidence directory.
- Platform evaluation: Windows/Linux use this shared implementation, but their native hosts were not run. The internal ARM64 reference was tested and packaged. Production macOS already uses Rust exact validation into typed `CivilDate` values in `ExactDatesView.apply`; no equivalent source correction was necessary. Native macOS UI and Swift suites passed.

### FUP-02 — Verifier-added JSON test necessarily targets the wrong Windows backend

- Severity: medium, affecting Windows diagnostics. Confidence: high from the mutually exclusive backend paths; native Windows reproduction remains unavailable.
- Trigger: run the earlier `test_ewaf_qt.cpp` on Windows. It writes a JSON file, constructs `DesktopPreferences`, and expects JSON-only values such as weekday `5`.
- Expected: regression assertions exercise the active storage backend. Actual: Windows `DesktopPreferences` reads registry values and never reads that JSON file, so those unconditional assertions cannot establish the intended behavior and fail for an isolated registry session.
- Fix: guard raw JSON assertions with `!defined(Q_OS_WIN)` and retain a native-backend write/reload assertion on all hosts. Normal test execution now uses a temporary configuration root and restores the temporary session setting. This also prevents JSON tests from depending on writable user preferences outside the test area.
- Source: `tests/qt/test_ewaf_qt.cpp:187`, native persistence case, and the `QT-PREF-01` JSON assertions. Production preferences code from the earlier pass is retained.
- Evidence: native Cocoa and offscreen suites each pass 22 scenarios; Windows branch/backend mismatch is established by source inspection, not by a claimed Windows run. Additional assertions cover immediate plan preservation with normalized dates and immediate invalidation after a date becomes invalid. Headless plugins explicitly skip only unsupported native style-hint assertions; palette and contrast assertions still run. Native Cocoa ran the style-hint assertions without skips.
- Platform evaluation: production macOS preferences are unchanged. Windows retains its registry backend and native reload coverage. Linux/internal-reference JSON tests remain active; Linux runtime is unverified.

### FUP-03 — Version diagnostic accepts stale committed copies

- Severity: low. Confidence: high; reproduced against the original checker in isolated fixtures.
- Trigger: align Cargo.toml and CMake but leave a local application package in Cargo.lock or any native Xcode build configuration on an older version. Running the original checker exits 0. Its CMake assertion also disappears under `python -O`.
- Expected: all committed application version copies match Cargo.toml, including under optimized Python. Actual: the original diagnostic only checks CMake and workspace-inheritance text, omitting Cargo.lock and Xcode versions.
- Fix: `script/check_versions.py:14` checks local application lockfile versions, TOML workspace inheritance, CMake, and both `CURRENT_PROJECT_VERSION` and `MARKETING_VERSION` on every native Xcode target/configuration. Explicit diagnostics replace assertions. Independently versioned `tlo-updater` remains 0.1.0.
- Regression: `tests/release/test_version_alignment.py` invokes the actual copied CLI against isolated valid, stale, missing, and independently versioned fixtures. Normal Python runs the regular cases; the CMake case explicitly uses `-O`.
- Evidence: the baseline checker produced seven expected failures across four selected test methods, with zero harness errors; the fixed checker passes all six methods. `version-baseline.log` includes the exact reproduction command/output. The complete release-tool suite passes 40 tests.
- Platform evaluation: this diagnostic reads committed metadata on every host. macOS package metadata was additionally validated. Native Windows/Linux package versions are implemented through their existing Cargo/CMake-derived scripts but were not packaged here. No workflow, required status check, release approval, or publication dependency was added.

## Focused reassessment of the earlier reports

| Earlier finding | Current assessment and limits |
| --- | --- |
| QT-UI-01 | Plain-text MIME payload is asserted with the real model. Retained. Cross-application native dragging remains manual acceptance. |
| QT-UI-02 | Menu states are asserted in normal and modal states; retained. This is UI availability evidence, not a new creation-safety rule. |
| QT-UI-03 | Clipboard fallback to the first preview row is asserted and matches the native macOS implementation. Retained. |
| QT-UI-04 | Explicit context-handler selection and clipboard result are asserted. A synthesized context-menu signal does not establish original physical right-click behavior on every native OS. Retained as deterministic handler behavior. |
| QT-UI-05 / IV-2026-10-30-02 | Normal and whitespace-wrapped dates preserve the full plan during in-flight filtering. A changed invalid range immediately disables creation. Retained; no additional workspace production change was needed. |
| QT-PREF-01 / IV-2026-10-30-03 | Real numeric/boolean/string JSON decoding is asserted on JSON backends. Prior test portability was corrected by FUP-02. Existing application writers store strings, so broad claims that normal settings were universally lost are not established. |
| QT-UI-06 | Picker bounds 0001–9999 are asserted; retained and extended by FUP-01. |
| WIN-INST-01 / IV-2026-10-30-01 | Current `MultiByteToWideChar(CP_UTF8, ...)` conversion is structurally appropriate. Current verification messages are ASCII; a current non-ASCII failing diagnostic path was not demonstrated. No Windows compiler/runtime was available, so earlier claims of native syntax/type verification are unsupported here. |
| WIN-INST-02 | Rejected as a demonstrated original defect: baseline `reset()` already excluded both null and `INVALID_HANDLE_VALUE`, and the sole boolean caller handles `OpenProcess`, whose failure is null. Added explicit boolean, move-assignment, and reset APIs are retained as benign hardening. |
| Historical validation totals | Superseded by the exact commands and observed totals below. A claim of 37 Swift tests or 32 total Rust tests does not describe this snapshot. |

This pass did not find another confirmed installer defect in the reviewed locking, authentication, MSI identity, coordinated handshake, process-wait, error, and cleanup paths. This is bounded source review, not Windows runtime acceptance or a claim that the repository is bug-free.

## Version and artifact changes

Cargo.toml owns 1.0.6. Cargo metadata refreshed only `ewaf-core`, `ewaf-ffi`, and `ewaf-update` local versions in Cargo.lock; registry dependencies did not change. CMake was updated and `python3 script/generate_xcode_project.py` regenerated the eight native version settings. Repeating the generator produced identical bytes. The dependency inventory check passed unchanged; dependency notices needed no update. Changelog and [1.0.6 notes](../releases/1.0.6.md) describe the combined work and its limits.

`./script/build_core.sh` ran before `swift test`, updated the ignored archive stamp, and allowed native relinking. No generated binaries or stamp headers are tracked. Application identity, native preferences, Codable dates, generated folder contents, updater authentication, reference isolation, and independent publication rules were preserved.

## Executed validation

All commands ran from the working directory stated above. `0` denotes the observed process exit status, not a cross-platform claim. Full logs are under `.local-reports/followup-2026-10-30/` and their hashes are in the snapshot manifest.

| Command | Exit and observed result | Evidence log |
| --- | --- | --- |
| `cargo fmt --all -- --check` | 0, clean | `rust-fmt.log` |
| `cargo clippy --workspace --all-targets --locked -- -D warnings` | 0, clean | `rust-clippy.log` |
| `cargo test --workspace --locked` | 0 on permitted rerun: 44 passed, 1 ignored | `rust-test-unsandboxed.log` |
| `./script/build_core.sh` | 0, refreshed Rust static archive/stamp before Swift | `build-core.log` |
| `swift test` | 0 on permitted rerun: 25 core + 18 app tests, 43 total | `swift-test-unsandboxed.log` |
| `./script/test_ui.sh` | 0, six native Xcode UI workflows | `macos-ui.log` |
| `env -u QT_QPA_PLATFORM ./script/test_qt.sh` | 0, 22 native Cocoa scenarios, native style hints tested | `qt-cocoa-after.log` |
| `QT_QPA_PLATFORM=offscreen ./script/test_qt.sh` | 0, 22 scenarios; native style hints explicitly skipped | `qt-offscreen-after.log` |
| `python3 -m unittest discover -s tests/release` | 0, 40 tests on final source | `release-tests-final.log` |
| `python3 -m unittest discover -s tests/linux -p '*test*.py'` | 0, four receipt tests on macOS, not Linux acceptance | `linux-receipt-tests.log` |
| `python3 script/check_versions.py` | 0, `Unified version: 1.0.6` | `versions.log` |
| `python3 script/check_icons.py` | 0, icon integrity/provenance checks | `icons.log` |
| `python3 script/generate_dependency_inventory.py --check` | 0, inventory current | `dependency-inventory.log` |
| `SPARKLE_SIGN_TOOL="$PWD/build/sparkle/2.9.6/bin/sign_update" cargo test --locked -p tlo-updater --test channel sparkle_and_shared_signer_interoperate -- --ignored` | 0, the normally ignored signer case passed with checksum-pinned tool and public deterministic test key | `sparkle-interop.log` |
| `CONFIGURATION=release ./script/build_and_run.sh --verify` | 0, ARM64 package built, deep signature integrity and ZIP validation passed, app launch/process smoke passed | `macos-package-and-launch.log` |
| `python3 script/inspect_native_package.py --tree dist/EWAF.app --platform macos --arch arm64 --package-file dist/EWAF-1.0.6-macos-arm64.zip --output .local-reports/followup-2026-10-30/macos-native-inventory.json` | 0, 105 observed entries; vendor provenance/license review remains separate | `macos-native-inventory.log` |
| `./script/package_qt_reference.sh` | 0, ABI reports 1.0.6; internal identity/ARM64 architecture/no-production-updater validation passed | `qt-reference-package.log` |
| Packaged internal executable `--ui-smoke`, through Python subprocess with a temporary `XDG_CONFIG_HOME` and 45-second timeout | 0, `Qt packaged window, bindings and core passed.` Native Cocoa; no production preference storage | `qt-reference-smoke.log` |
| `codesign --verify --deep --strict 'dist/EWAF Qt Internal.app'` | 0, ad-hoc integrity | verifier execution |
| `git -c core.fsmonitor=false diff --check` | 0 | final verification |

The first sandboxed Rust test attempt exited 101 because two local mock servers could not bind loopback sockets. The first Swift attempt exited 1 because compiler caches were outside writable roots. Each passed after one environment-permitted rerun; source fixes were not needed for these restrictions. Initial sandboxed Qt attempts hit user-config write denial/no-screen access; tests now isolate config storage and native Cocoa passed with GUI access. Those initial failures are not hidden or attributed to passing platforms.

`python3 script/check_update_trust.py` exited 1 with `Owner must enroll an EWAF public update key before a stable release`. Enrollment remains absent and authenticated production updating fails closed. This is existing production-updater configuration, not a reason to weaken verification or block the unsigned/ad-hoc direct-download packaging model. No owner credentials or enrollment changes were requested or made.

Local package SHA-256:

- `dist/EWAF-1.0.6-macos-arm64.zip`: `98e75b28a44b2f30949596859cd44d9400ddfe85cb69495e0e01ac59b891b9c2`.
- `dist/internal-qt-reference-1.0.6-osx-arm64.zip`: `ab4a1da6a371ba085aeea91d5237337e1ccc92636246d7b01e64f4964c32de09`. Internal only; never a production release/update asset.

## Independent focused review and remaining work

The primary Codex context read the previous artifacts, owned version integration/documentation and root validation, and reviewed the new Qt/version edits. Distinct Codex subagent contexts performed the Qt fix-and-test pass (`qt_followup`), installer source reassessment (`installer_followup`), and release/version review and fix (`release_review`). `installer_followup` then independently reviewed the newly authored version checker/tests and Qt follow-up without editing them; it found no actionable defect and independently ran the six version tests and current diagnostic successfully. These were separate contexts using the inherited configured model, not a claim that a different model family verified the work. No simultaneous agent edits targeted the same files, and no further full audit loop was started.

Coverage includes the changed Qt handlers, plan invalidation/search callers, preferences backend differences, Swift exact-date/workspace presentation, Windows installer error/handle/lifecycle paths, version propagation, package validation, and scoped release/retry-policy review. Full Rust/Swift/shared Qt/tooling tests were executed. There was no new exhaustive repository-wide security review.

Native Windows x64/ARM64, Linux x64/ARM64, and production Intel macOS packages remain untested in this pass. Real Windows Unicode dialogs/MSI coordination, Linux AppImage/FUSE/dconf behavior, native assistive technology, minimum-OS, mixed-DPI, and cross-application dragging still require their own native acceptance. Atomic-preferences commit-failure injection was not added or executed. No failing platform was reported as passed; no CI run or remote issue was created. Package binary inventory is observed evidence, not completed vendor-license or security-patch qualification.

No significant behavior compromise or compatibility break was introduced, and no owner decision is needed for these local fixes. Future authenticated-updater enrollment remains an owner operation. The implementation and local verification described here are complete; platform acceptance gaps remain explicit.
