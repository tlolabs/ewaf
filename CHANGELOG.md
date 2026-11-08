# Changelog

All notable changes to EWAF will be recorded here. Entries follow [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and [Semantic Versioning](https://semver.org/). Existing Git history and GitHub Releases remain the record for earlier versions.

## [Unreleased]

## [1.0.6]

### Fixed

- Qt preview text dragging, copy fallback, context-menu selection, menu availability, search-plan preservation, and full-range calendar selection.
- Calendar pickers now retain valid exact dates with surrounding whitespace, using the shared Rust parser.
- JSON preferences retain numeric and boolean values; regression tests exercise each platform's actual preferences backend.
- Windows installer error dialogs use UTF-16 conversion.
- Version diagnostics detect stale application versions in Cargo.lock and every native Xcode build configuration.

See the [1.0.6 release notes](docs/releases/1.0.6.md) and [follow-up verification](docs/audits/2026-10-30-follow-up-and-version-1.0.6.md) for evidence and platform limitations.

## [1.0.5]

### Changed

- Standardized repository documentation and recorded the unresolved licensing and Windows redistribution review.
- Reduced allocations in shared date calculations, search previews, and macOS date bridging without changing folder creation behavior.

See the [1.0.5 release notes](docs/releases/1.0.5.md) for platform and verification status.

## [1.0.4]

See the [1.0.4 release notes](docs/releases/1.0.4.md). Historical changes are not reconstructed here.

[Unreleased]: https://github.com/tlolabs/ewaf/compare/v1.0.6...HEAD
[1.0.6]: https://github.com/tlolabs/ewaf/compare/v1.0.5...v1.0.6
[1.0.5]: https://github.com/tlolabs/ewaf/compare/v1.0.4...v1.0.5
[1.0.4]: https://github.com/tlolabs/ewaf/releases/tag/v1.0.4
