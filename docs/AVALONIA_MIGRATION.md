# Avalonia migration qualification

Migration baseline: `300a2b28594e602181e4ec970eeacd40f975a69e` (WinUI/GTK updater qualification). Working branch: `codex/avalonia-migration`. Final implementation validation: `5a9024e7332d77022942bda97065eff4ab44dd48`, [successful CI run 36873964676](https://github.com/tlolabs/ewaf/actions/runs/36873964676), 2026-10-01. Subsequent qualification-record edits change documentation only. Engineering validation is successful; distribution remains blocked as described below.

## Architecture and scope

Before: production SwiftUI/AppKit, WinUI 3 and GTK 4/libadwaita, all consuming the shared Rust core. After: production SwiftUI/AppKit remains intact; one `platform/avalonia/EWAF` project supplies all AXAML, resources, window code, workspace state and commands to Windows x64/ARM64, Linux x64/ARM64 and internal Mac ARM64. Rust core/FFI/updater algorithms were not rewritten. The existing C# FFI binding was moved with only whitespace normalization. Windows native MSI verifier and Rust update helper remain OS services below the shared presentation.

Pinned inputs: Avalonia 12.1.3, .NET SDK 10.0.401/runtime 10.0.12; SkiaSharp 3.119.4, HarfBuzzSharp 8.3.1.3, ANGLE 2.1.27548.20260419, MicroCom.Runtime 0.11.6, Tmds.DBus.Protocol 0.94.1. Headless 12.1.3 and its Inter font package are test-only. Avalonia.BuildServices 11.3.2 is build-only; build telemetry is disabled in the entry points and CI. Lockfiles and audited notice provenance are committed. No paid service or commercial Avalonia control is used.

Removed: WinUI application source/project/lock/resources and GTK source/widgets/schema/resource build paths. Retained: Windows packaging/installer, Linux desktop metadata, all core and native Mac source/tests. Added: shared workspace/dialog/preferences/update services, AXAML, internal packaging validator, headless regression harness and release-isolation test.

## Feature audit against the baseline

| Capability | Replacement / preserved behavior | Evidence and limits |
| --- | --- | --- |
| Date entry, historical range, calendar picker, localized weekdays | Shared exact fields/calendar pickers; Rust performs exact validation and plans | Shared tests; actual internal Mac date entry inspected; OS calendar selection/manual minimum OS pending |
| Preview/order/search/200-row bound/full plan | Same Rust plan and search; stale async responses discarded | Shared Unicode search/full-plan and stale-response tests; existing Rust tests |
| Create/destination/reveal | Avalonia StorageProvider; OS file manager adapter; session-only destination | Shared creation/chooser-cancel tests; interactive native chooser/reveal acceptance pending |
| Existing folders, conflicts, partial completion/retry | Unchanged Rust capabilities and failure summaries | Shared real filesystem retry/conflict tests, existing core/FFI tests |
| Progress/cancellation/window closure | Progress marshaled to dispatcher; token cancellation; process remains until closing work releases handles | Shared cancellation and lifecycle tests; queued progress cannot overwrite completion |
| Large-operation warning | Rust threshold; shared modal with Cancel default focus | Shared tests and actual internal Mac 574-folder confirmation/cancel verified |
| Settings/default weekday/automatic updates | Shared settings; Windows existing HKCU; Linux dconf import then atomic JSON; separate internal Mac storage | Shared persistence/type-decoding/settings tests; actual dconf import and one-time JSON persistence pass on both Linux architectures |
| Multiwindow/last range/geometry | Independent workspace per window; last range/dimensions retained; destination not persisted | Shared independence/restoration; two-window updater guard |
| Menus/keyboard/help/about | File/Edit/Help, New/Choose/Create/Cancel/Settings/Close, Ctrl shortcuts and reference Mac Command equivalents | AXAML/headless focus; actual internal Mac settings shortcut verified; full Windows/Linux keyboard acceptance pending |
| Copy/text drag | Shared clipboard context action/Ctrl-C and text drag | Shared clipboard write/read regression passes; cross-application drag/manual clipboard acceptance pending |
| Updates | Shared prompts/scheduling; unchanged Rust trust, Windows MSI handshake, Linux AppImage transaction; native Mac Sparkle unchanged | Existing Rust/native verifier tests retained; shared busy-window guard; production signed A→B remains blocked by existing enrollment |
| Appearance/DPI/accessibility | Fluent system light/dark, scalable layout, scrollable form, labels/automation IDs, polite status live region, keyboard focus | Actual internal Mac AX tree/visual inspection and shared controls test; Narrator/Orca/VoiceOver, contrast/multimonitor manual checks pending |
| CLI | `--core-smoke` retains ABI/version prefix; `--ui-smoke` validates packaged shared window and exits with isolated prefs | Internal package smoke and Linux CI path; no other user CLI existed |

Production macOS UI, shortcuts, scene storage, Codable dates, Sparkle and bundle identity remain unchanged. Its native sharing and appearance have not been reduced to match Avalonia.

## Final validation and actual builds

The resulting implementation passed all eight required non-release jobs in the linked CI run. These are real architecture-specific runners, not inferred compatibility. Tag-only `macos-icons` and `release` jobs were skipped as expected; no production release was performed.

| Target / check | Exact result | Build/package evidence |
| --- | --- | --- |
| Quality | VERIFIED: Rust fmt/clippy, 34 Rust tests, 5 release/isolation tests, 4 Linux receipt tests; version/icon/inventory/notices checks pass | Locked Rust 1.98.1; application version remains 1.0.5 |
| Windows x64, Windows Server 2022 | VERIFIED: 25 Rust tests; C#/Rust integration; shared harness reports 12 scenarios (Mac-specific branch inapplicable); real UI Automation launch, accessible controls, invalid dates and large confirmation; native MSI signature/digest/publisher/version/architecture/abort checks; actual MSI installation, major upgrade, forced rollback, removal; per-user install/remove preserves contents | Self-contained ZIP validated and MSI built |
| Windows ARM64, Windows 11 ARM | VERIFIED: same independent suite and package checks on ARM64, zero failures | ARM64 self-contained ZIP and MSI built |
| Linux x64, Ubuntu 24.04 | VERIFIED: 34 Rust tests; shared harness reports 12 scenarios (Mac-specific branch inapplicable); real dconf import/persistence; three real-window smoke paths: build, AppImage, installed Debian package | x64 .deb and AppImage built; .deb installed; ELF/desktop/icon checks pass |
| Linux ARM64, Ubuntu 24.04 ARM | VERIFIED: same independent suite and package checks on ARM64, zero failures | ARM64 .deb and AppImage built and exercised |
| Internal macOS ARM64 Avalonia | VERIFIED: all 12 shared scenarios including updater rejection; real packaged window smoke; ARM64/identity/no-updater archive validator | Ad-hoc app/ZIP built locally and in CI; [downloadable internal artifact](https://github.com/tlolabs/ewaf/actions/runs/36873964676/artifacts/11167514074), retained 14 days |
| Native macOS ARM64, macOS 26 | VERIFIED: 34 Rust tests; 33 Swift tests; all 6 XCTest UI workflows; generated Xcode project drift check; one additional Sparkle/shared-signer interoperability test | Native release package, architecture/signature/icon validation pass; local native build/run verification also passes |
| Native macOS x64, macOS 15 | VERIFIED: same 34 Rust, 33 Swift, 6 XCTest UI and one explicit Sparkle interoperability test independently pass | Native x64 release package validated |

The Sparkle interoperability test is initially ignored by the default Mac Rust invocation, then explicitly executed after packaging on each Mac runner. Windows lacks Unix-only AppImage transaction tests, so its Rust total is 25 rather than 34. Shared test totals do not turn the conditional internal-Mac assertion into Windows/Linux evidence.

Local final checks additionally pass: actionlint, `git diff --check`, exact version/dependency notice/inventory checks, internal package validation and actual packaged window smoke. The complete diff was reviewed against the baseline: no changes to `crates/`, `bindings/`, production `platform/macos/`, `Package.swift` or `Package.resolved`; no generated binaries, downloaded SDK/source, logs, build-stamp headers or credentials are tracked. Commits are unsigned with DCO sign-offs, pushed to `codex/avalonia-migration`.

## CI, packaging and dependency review

Existing Windows/Linux package entry points now build the canonical Avalonia project and collect exact locked vendor notices. Windows retains its native installer and MSI qualification. Linux packages the self-contained app under `/usr/lib/ewaf` with the established launcher/desktop identity; AppImage retains the authenticated Rust update transaction. Native production Mac packaging remains intact. The internal Mac bundle is a separate packaging entry point and artifact family.

CI pins .NET, runs the shared harness on all five Avalonia targets, tests actual Windows UI Automation and actual Linux windows, retains native Mac tests, and makes the internal reference job a release prerequisite. Non-tag runs do not publish production packages; the internal reference artifact is always downloadable. All platform packages use the same Cargo version and source commit.

Exact NuGet package versions/hashes, upstream provenance, full vendor notices and test/build-only dependencies are recorded in `licenses/avalonia/packages.json`, `dependency-inventory.json` and `THIRD_PARTY_NOTICES.md`. The removed WinUI binary dependencies are absent. The audit found Ms-PL source components inside Avalonia.Controls despite the top-level MIT package declaration: **BLOCKED pending owner approval of the narrow additional permission in [AVALONIA_LINKING_PERMISSION_PROPOSAL.md](AVALONIA_LINKING_PERMISSION_PROPOSAL.md)**. That draft is not an operative grant. See [LICENSE_AUDIT.md](LICENSE_AUDIT.md). No assertion of cleared distribution licensing is made.

Existing production signing/trust enrollment is separately absent. Authenticated production older-to-newer update qualification, notarization and production signer receipts remain BLOCKED; test certificates and development artifacts are not production evidence. No tag or production release was created.

## Structural internal isolation

Internal Mac uses `com.tlolabs.ewaf.avalonia-internal`, separate storage, a visibly INTERNAL title, `INTERNAL_REFERENCE` compiled for osx-arm64, plus a runtime macOS updater guard. Its bundle contains neither ewaf-update nor Sparkle, installer, feed or public trust metadata. Its ZIP and CI artifact use `internal-avalonia-reference-*`, which cannot match production `EWAF-*` artifact collection. Production assembly rejects internal/avalonia-named inputs before signing. Production native Mac receipt validation also requires the original bundle ID and Sparkle metadata. The internal job is a required release dependency, never a published production asset.

## Accessibility and input

Shared controls expose labels, automation IDs, text/selection semantics, progress and polite live status. The form precedes the footer in logical keyboard order; opening focus goes to Start date, dialogs default to Cancel, and the headless suite exercises Tab, Find, clipboard, settings and confirmation. Windows UI Automation verifies real accessible controls on both architectures. Local reference inspection verified its accessibility tree, date workflow, 574-folder warning/cancel and Command-comma Settings.

Pinned Avalonia source confirms TextBlock text changes raise Name events and the Windows backend converts them to live-region events when `LiveSetting` is enabled. Linux uses the framework's [AT-SPI2 bridge](https://docs.avaloniaui.net/docs/platform-specific-guides/linux), but this is implementation evidence, not completed Orca acceptance. No screen-reader speech output or full high-DPI/contrast acceptance is claimed.

## Manual acceptance remaining

Test native folder dialogs/reveal and clipboard/text drag with other apps on both production OSes; Narrator/Orca and keyboard-only workflows; high contrast, large text, fractional DPI and multiple monitors; minimum Windows 10 1809 / ARM64 supported device acceptance and Wayland via XWayland. Avalonia upstream tiers differ from the retained product compatibility target; CI on Windows Server 2022/Windows 11 ARM64 and Ubuntu 24.04 does not verify older Windows devices. Native Wayland is experimental and not enabled. No production signed update qualification is claimed.

The first platform CI run passed the internal reference job and Linux shared tests/real-window/Debian packaging on both architectures, then caught an AppImage dependency scan against .NET's optional `libcoreclrtraceptprovider.so`. That provider targets obsolete `liblttng-ust.so.0`; AppImage packaging now omits only that optional LTTng diagnostic provider. CoreCLR/EventPipe, application features and native updater remain present. See [upstream ABI issue](https://github.com/dotnet/runtime/issues/57784). The corrected AppImages passed their own build and actual-window smoke on both Linux architectures in the final run above.
