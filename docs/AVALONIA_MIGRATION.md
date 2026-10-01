# Avalonia migration qualification

Migration baseline: `300a2b28594e602181e4ec970eeacd40f975a69e` (WinUI/GTK updater qualification). Working branch: `codex/avalonia-migration`. This record is updated with results as validation completes; pending items are not passes.

## Architecture and scope

Before: production SwiftUI/AppKit, WinUI 3 and GTK 4/libadwaita, all consuming the shared Rust core. After: production SwiftUI/AppKit remains intact; one `platform/avalonia/EWAF` project supplies all AXAML, resources, window code, workspace state and commands to Windows x64/ARM64, Linux x64/ARM64 and internal Mac ARM64. Rust core/FFI/updater algorithms were not rewritten. The existing C# FFI binding was moved unchanged. Windows native MSI verifier and Rust update helper remain OS services below the shared presentation.

Pinned inputs: Avalonia 12.1.3, .NET SDK 10.0.401/runtime 10.0.12; SkiaSharp 3.119.4, HarfBuzzSharp 8.3.1.3, ANGLE 2.1.27548.20260419, MicroCom.Runtime 0.11.6, Tmds.DBus.Protocol 0.94.1. Headless 12.1.3 is test-only. Lockfiles and audited notice provenance are committed. No paid service or commercial Avalonia control is used.

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
| Settings/default weekday/automatic updates | Shared settings; Windows existing HKCU; Linux dconf import then atomic JSON; separate internal Mac storage | Shared persistence/type-decoding/settings tests; real Linux migration verification pending |
| Multiwindow/last range/geometry | Independent workspace per window; last range/dimensions retained; destination not persisted | Shared independence/restoration; two-window updater guard |
| Menus/keyboard/help/about | File/Edit/Help, New/Choose/Create/Cancel/Settings/Close, Ctrl shortcuts and reference Mac Command equivalents | AXAML/headless focus; actual internal Mac settings shortcut verified; full Windows/Linux keyboard acceptance pending |
| Copy/text drag | Shared clipboard context action/Ctrl-C and text drag | Implemented; cross-application drag/manual clipboard acceptance pending |
| Updates | Shared prompts/scheduling; unchanged Rust trust, Windows MSI handshake, Linux AppImage transaction; native Mac Sparkle unchanged | Existing Rust/native verifier tests retained; shared busy-window guard; production signed A→B remains blocked by existing enrollment |
| Appearance/DPI/accessibility | Fluent system light/dark, scalable layout, scrollable form, labels/automation IDs, polite status live region, keyboard focus | Actual internal Mac AX tree/visual inspection and shared controls test; Narrator/Orca/VoiceOver, contrast/multimonitor manual checks pending |
| CLI | `--core-smoke` retains ABI/version prefix; `--ui-smoke` validates packaged shared window and exits with isolated prefs | Internal package smoke and Linux CI path; no other user CLI existed |

Production macOS UI, shortcuts, scene storage, Codable dates, Sparkle and bundle identity remain unchanged. Its native sharing and appearance have not been reduced to match Avalonia.

## Current validation status

- VERIFIED locally: Rust fmt/clippy and workspace tests; 33 Swift tests and all 6 native XCTest UI workflows; normal native release-build packaging/signature/launch; 12 shared presentation/FFI/headless scenarios; 5 release configuration/isolation tests; 4 Linux receipt tests; internal Mac ARM64 self-contained publish, ad-hoc bundle, ZIP and identity/updater-exclusion validation; actual reference window date entry, accessibility tree, large confirmation/cancel and Settings shortcut.
- NOT VERIFIED yet: resulting-commit Windows x64/ARM64 and Linux x64/ARM64 CI; fresh native Mac packaging/XCTest CI; final artifact/license inventory and full final diff.
- BLOCKED pending owner decision: additional GPL linking permission for verified Ms-PL components within Avalonia.Controls. See LICENSE_AUDIT.md and AVALONIA_LINKING_PERMISSION_PROPOSAL.md. Existing production signing/trust enrollment is separately absent; no production tag or release is authorized or created.

## Structural internal isolation

Internal Mac uses `com.tlolabs.ewaf.avalonia-internal`, separate storage, a visibly INTERNAL title, `INTERNAL_REFERENCE` compiled for osx-arm64, plus a runtime macOS updater guard. Its bundle contains neither ewaf-update nor Sparkle, installer, feed or public trust metadata. Its ZIP and CI artifact use `internal-avalonia-reference-*`, which cannot match production `EWAF-*` artifact collection. Production assembly rejects internal/avalonia-named inputs before signing. Production native Mac receipt validation also requires the original bundle ID and Sparkle metadata. The internal job is a required release dependency, never a published production asset.

## Manual acceptance remaining

Test native folder dialogs/reveal and clipboard/text drag with other apps on both production OSes; Narrator/Orca and keyboard-only workflows; high contrast, large text, fractional DPI and multiple monitors; minimum Windows 10 1809 / ARM64 supported device acceptance and Wayland via XWayland. Avalonia upstream tiers differ from the retained product compatibility target; CI on Windows Server 2022/Windows 11 ARM64 and Ubuntu 24.04 does not verify older Windows devices. Native Wayland is experimental and not enabled. No production signed update qualification is claimed.

The first platform CI run passed the internal reference job and Linux shared tests/real-window/Debian packaging on both architectures, then caught an AppImage dependency scan against .NET's optional `libcoreclrtraceptprovider.so`. That provider targets obsolete `liblttng-ust.so.0`; AppImage packaging now omits only that optional LTTng diagnostic provider. CoreCLR/EventPipe, application features and native updater remain present. See [upstream ABI issue](https://github.com/dotnet/runtime/issues/57784). The corrected packages require their own successful runner evidence.
