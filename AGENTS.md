# EWAF development rules

- Read docs/BEHAVIOR.md and docs/PARITY.md before changing a workflow. Rust in crates/ewaf-core is authoritative for platform-independent behavior. Do not reproduce date, naming, filtering, confirmation, conflict or recovery rules in Swift, C# or GTK code.
- Preserve SwiftUI on macOS, WinUI 3 on Windows and GTK 4/libadwaita on Linux. Native layers own controls, accessibility, dialogs, lifecycle, scheduling and UI/preferences storage.
- Every intentional user-facing change must explicitly evaluate all three platforms, implement native equivalents, update the parity matrix and add meaningful regression coverage. Document implementation versus verified status. Ask the owner before a significant behavioral compromise or compatibility break.
- Preserve com.tlolabs.ewaf, defaultWeekday, rangeStart, rangeEnd, rangeWeekday, Codable dates and generated directory contents. The owner authorized legacy removal on 2026-09-16; the references live on codex/archive-legacy-1.0.2. The owner also authorized removal of all Python-derived fixture data; keep legacy code and data out of the active tree.
- Run ./script/build_core.sh before swift test. Rust archive changes must update the generated C build-stamp header so SwiftPM/Xcode relink. Never commit generated binaries, build-stamp headers or credentials.
- Run cargo fmt/clippy/tests with the locked toolchain, applicable native tests, version checks and package validation. A failed Windows or Linux job is a regression even if macOS passes.
- Cargo.toml owns the application version. Update/check native copies and regenerate dependency notices/locks when dependencies change. Release all platform artifacts from one commit, only after required validation succeeds.
- Use script/build_and_run.sh for macOS build/run and the platform scripts for packaging. Development artifacts must work without signing credentials; optional credentials come from Keychain or secure CI secrets.
