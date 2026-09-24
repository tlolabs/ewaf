# EWAF 1.0.3

EWAF now uses one shared Rust core with native SwiftUI on macOS, WinUI 3 on Windows and GTK 4/libadwaita on Linux. The release preserves inclusive weekly folder generation, historical exact dates, chronological/searchable previews, content-preserving creation, cancellation, recovery and native preferences.

The legacy implementations are archived at version 1.0.2 on codex/archive-legacy-1.0.2. Version 1.0.3 removes them from the active application tree while retaining the shared calendar regression fixtures and native test coverage.

Packages support macOS 14+ (Apple Silicon and Intel), Windows 10 1809+ (x64 and ARM64), and Ubuntu 24.04 (amd64 and arm64). Download the package for your platform and architecture and consult README.md for installation. Existing generated folders and macOS preferences retain their formats and identifiers.

Release artifacts are gated on Rust quality checks and all six native build/test/package jobs from the release commit. macOS development signing is ad-hoc and Windows packages may be unsigned while owner credentials are unavailable. Screen-reader, display/scaling, minimum-OS, interactive Windows/Linux picker and real-credential signing acceptance have not all been performed; the owner authorized this release with those limitations documented in docs/PARITY.md.
