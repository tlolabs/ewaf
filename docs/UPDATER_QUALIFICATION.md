# Updater qualification record

Status on 2026-10-01: **not end-to-end qualified; no release tag authorized by the evidence below**. This record accompanies the `codex/updater-qualification` branch. Version remains 1.0.5. The complete development native matrix now passes on `380ca0481cb0af8de520b6fa695b461e483c05f9`. Production enrollment and a real signed A-to-B transition remain outstanding. A subsequent documentation-only commit records these results; it changes no tested executable source or workflow.

## Audit and fixes

Reviewed the updater diff and surrounding scheduling, lifecycle, packaging, identity, release, and dependency code on all three platforms. The original-snapshot Codex Security scan `caa21465-fdef-4f87-ace6-8fb6cf1dbd95` reported a medium-severity AppImage rollback issue: an old still-running process compared against its compiled version after a newer image had already been installed. Source validation confirmed the issue; an executable exploit reproduction was not completed. The fix uses a persistent transaction lock and durable monotonic version/digest journal before replacement, with tests for stale processes, same-version substitution, interrupted commit recovery, locking, unsafe paths and incorrect ELF architecture. The sealed scan describes the original snapshot, not the final patched tree. Coverage remains partial because native production installation has not run.

Other fixes include native Windows Authenticode/MSI verification and a READY/COMMIT handshake before closing the app; test coverage for MSI rollback, identity and cancellation; correct GTK active-dialog and multiwindow guards; verifying the helper actually embedded inside the AppImage; packaged GTK icon/schema/window smoke checks; removal of stale development Sparkle configuration; production release-configuration enforcement; and inclusion of the previously ignored release-signing CLI source. No signing, notarization, test or publication gate was weakened.

The macOS trust path uses pinned Sparkle signed feeds and archives plus Apple signing. Windows/Linux use the installed public Ed25519 key to authenticate the exact manifest payload before interpreting artifact URLs, identities, versions, length and SHA-256. Windows additionally verifies native signature and MSI identity before closing. Linux replacement consumes the authenticated candidate and verifies staged bytes and architecture. Checksums alone do not authorize installation. Production metadata requires all six same-commit native receipts and configured signing identities.

## Local evidence

| Check | Result |
| --- | --- |
| Locked Rust workspace tests | 34 passed; the external Sparkle interoperability test is separately exercised below |
| Rust formatting and Clippy, warnings denied | Passed |
| Official pinned Sparkle signer interoperability | 1 passed explicitly |
| Swift unit/integration/lifecycle tests | 33 passed, after building the Rust archive; Xcode 26.6 selected explicitly |
| Release configuration tests | 4 passed, including invalid trust under Python optimization |
| Linux native-receipt tests | 4 passed; invalid signatures are rejected before executing image contents |
| Version, icons, dependency inventory/notices, actionlint, diff whitespace | Passed |
| macOS release-configuration package and launch | Passed via `CONFIGURATION=release ./script/build_and_run.sh --verify`; development channel/ad-hoc signing |
| macOS XCTest UI suite | First run: 3 passed, 3 failed. Two subsequent attempts failed before tests with `Timed out while enabling automation mode`, including after a test-service restart. Not passed or waived. |
| GTK widget suite on this Mac | Earlier baseline passed; expanded lifecycle suite terminates on fatal GDK warning `gdk_frame_timings_presented() called on skipped frame`. Native Linux CI required; warning remains fatal. |
| Windows native tests | No local Windows runner/toolchain; both native CI architectures now pass (below) |

Local artifact: `dist/EWAF-1.0.5-macos-arm64.zip`, containing `com.tlolabs.ewaf` 1.0.5 and embedded Sparkle. SHA-256: `fac6eeb09fad09940667809c1bc170e8087705fde095afc28aeba9d7f83abd1b`. Deep/strict nested signature validation passes; `codesign` reports ad-hoc signing, no team identifier, and the main executable has no hardened-runtime flag. Gatekeeper assessment rejects this local development artifact. This is an ad-hoc signed development-channel artifact, **not a production artifact or VERSION A**. Manual native UI checks confirm that the disabled/unconfigured updater reports its status and leaves the app usable. These checks do not demonstrate discovery, download, installation or relaunch.

Logs are local ignored files under `build/qualification-*.log`; XCTest results are under `dist/UITests*.xcresult`. Formal scan artifacts are outside the repository under the Codex state directory. The scan service reported measured token usage of 7,037,302 total (7,007,542 input, 6,616,960 cached input), with complete usage coverage across four threads. These are service-reported totals, not estimates.

## Native CI checkpoint

Checkpoint `43f16e8a5dfc2ff052aec1c5e7d76d24dc5aaa75` was pushed with DCO sign-off. Cryptographic commit signing was attempted but GPG could not open its interactive passphrase prompt; the commit is unsigned, consistent with the repository default and the request's signing-unavailable exception.

| Native run | Tested commit | Result |
| --- | --- | --- |
| [Initial matrix](https://github.com/tlolabs/ewaf/actions/runs/36829423470) | `43f16e8a5dfc2ff052aec1c5e7d76d24dc5aaa75` | Quality and both macOS jobs passed, including 33 Swift and six UI tests per architecture, package validation and Sparkle signer interoperability. Both Linux jobs exposed a test-selector bug: the confirmation test selected a prior creation-result dialog. Windows was cancelled after stalling in fixture-certificate setup; not counted as passing. |
| [Second matrix](https://github.com/tlolabs/ewaf/actions/runs/36830731141) | `76001b26262c91049ce8b48f3461d618d5969f47` | Quality and both macOS jobs passed again. Both native GTK lifecycle suites and Debian package validations passed. Both AppImages failed their actual packaged launch because writing `AppRun` followed linuxdeploy's symlink and overwrote the executable. Windows reached a bounded timeout while enrolling a fixture root certificate. |
| [Third matrix](https://github.com/tlolabs/ewaf/actions/runs/36832092152) | `d863e007b28328d64330a5cb29c96cd7bfb5cc61` | Both macOS and both Linux jobs passed. Windows passed all native trust/identity and READY/ABORT assertions, but the harness incorrectly propagated the last expected negative-test exit code. The script now returns success only after every assertion and cleanup completes; exceptions still fail. |
| [Final development matrix](https://github.com/tlolabs/ewaf/actions/runs/36833065508) | `380ca0481cb0af8de520b6fa695b461e483c05f9` | **Success: quality plus all six native jobs passed.** Production-only signing, cross-catalog release validation and publication jobs are not exercised by a development branch run. |

The GTK test now identifies each dialog by its response ID, retaining all assertions. AppImage packaging now removes linuxdeploy's `AppRun` symlink before writing the wrapper; the real packaged core and window/resource smoke checks remain required. Windows test setup now requires a disposable GitHub-hosted runner and enrolls its unique fixture root in the administrative machine store, removing that exact thumbprint afterward. Current-user enrollment waited for desktop consent even through certutil. The installed application's WinVerifyTrust and revocation checks are unchanged; no production certificate is used by these fixtures.

Final CI evidence:

- Shared quality: formatting, warnings-as-errors Clippy, release CLI build, 34 Rust tests on Unix, four release configuration tests, four native receipt tests, version/icons, dependency inventory and regenerated notices passed.
- macOS ARM64 and x64: 33 Swift unit/integration/lifecycle tests and all six XCTest UI tests per architecture passed, plus package validation and the separately invoked official Sparkle/shared-signer interoperability test. The earlier local XCTest failures remain recorded above; they were not waived or hidden.
- Windows ARM64 and x64: 25 applicable Rust tests, real native signature/identity negative cases and READY/ABORT handshake, actual MSI installation and major upgrade, deliberately failed deferred transaction with rollback to the prior version, uninstall with user-content preservation, C#/Rust integration, WinUI accessibility/date-validation/confirmation smoke, and existing per-user install/remove tests passed. These are isolated test certificates and MSI fixtures, not Azure-signed production updater transitions.
- Linux ARM64 and x64: 34 Rust tests, the expanded native GTK lifecycle suite, Debian package validation/install, and actual AppImage core plus native window/settings/icon-resource smoke passed. These are development AppImages, not production GPG/provenance or a published signed A-to-B update.

Warnings observed: Xcode leaves already-signed Sparkle/XCTest binaries unstripped and skips App Intents metadata for targets without App Intents; GitHub reports older JavaScript action runtime deprecations; Xvfb reports unavailable DRI3 acceleration; AppImage tooling reports missing optional AppStream metadata. These are recorded separately from the fatal local GDK warning and failed native qualification attempts. EWAF has no media-processing test suite.

## Production prerequisites and acceptance

Read-only checks found no repository Actions secrets, no repository Actions variables and no GitHub environments for `tlolabs/ewaf`. `updates/trust.json` deliberately contains no enrolled update keys or platform identities. A local Developer ID Application identity for Thomas Lothian, team `VR64M92P2M`, and the configured maintainer GPG signing key are available; their presence does not supply the missing CI configuration or establish notarization.

Owner-controlled enrollment must provide:

- An EWAF-specific Ed25519 public key in `updates/trust.json`, with its matching seed protected as `TLO_UPDATE_SIGNING_SEED`. Do not reuse a sibling application's key or put private material in Git.
- The approved Apple team ID in public trust, and protected `MACOS_CERTIFICATE_P12_BASE64`, `MACOS_CERTIFICATE_PASSWORD`, `MACOS_KEYCHAIN_PASSWORD`, `MACOS_SIGNING_IDENTITY`, `NOTARY_APPLE_ID`, `NOTARY_APP_PASSWORD`, and `APPLE_TEAM_ID` for the production workflow.
- The approved exact Windows certificate publisher in public trust; `AZURE_CLIENT_ID`, `AZURE_TENANT_ID`, `AZURE_SUBSCRIPTION_ID` secrets; and `AZURE_SIGNING_ENDPOINT`, `AZURE_SIGNING_ACCOUNT`, `AZURE_SIGNING_PROFILE` variables for the existing Azure signing account.
- The approved Linux GPG fingerprint in public trust, `LINUX_GPG_PRIVATE_KEY` and `LINUX_GPG_PASSPHRASE` secrets, and `LINUX_SIGNING_KEY` variable.
- Resolution by the owner of the existing Windows distribution hold in `docs/LICENSE_AUDIT.md`, recorded by `WINDOWS_DISTRIBUTION_APPROVED=true` only after that resolution. No legal approval is inferred from an updater implementation request.
- Protected production environment and reviewed tag/signing access. Windows/Linux jobs require repository-accessible credentials as currently authored; macOS final signing executes in the production environment.

All required native gates must pass on the exact intended release commit before tagging. Then use the normal signed annotated stable-tag workflow, inspect all six signed artifacts/receipts and metadata in its draft, and publish only after the applicable approval and validation requirements are met. The current pipeline cannot create a macOS-only stable update by ignoring Windows/Linux failures.

The latest published release observed is v1.0.4 and has no updater. VERSION A must therefore be an enrolled, production-signed updater-capable bridge, installed manually once; VERSION B must be a genuinely newer production release. Neither version has been established here. Do not represent v1.0.4 or the development artifact as VERSION A. Do not fabricate a tag to work around missing credentials.

| Required production outcome | Evidence |
| --- | --- |
| Tested source commit / native CI | `380ca0481cb0af8de520b6fa695b461e483c05f9`; quality and all six development jobs passed; no production release commit declared qualified |
| Release tag / GitHub Release | None created |
| VERSION A / VERSION B | Not established |
| Production macOS signing / hardened runtime / nested signatures | Not qualified; local ad-hoc package only |
| Notarization / staple / Gatekeeper | Production checks not performed |
| GitHub updater discovery / authenticated download | Not performed on a published production release |
| Updater installation / relaunch / preserved settings and folders | Not performed |
| Post-update regression / same-version recheck | Not performed |

**VERSION A → GitHub discovery → authenticated download → installation → relaunch → VERSION B → passing regressions has not succeeded.** Missing production enrollment and the existing Windows distribution hold prevent the normal stable release pipeline from establishing the required bridge and newer release. The full development native matrix passes; it does not replace production signing or the requested update transition. Windows and Linux also require production signing and signed older-to-newer installation evidence. Neighboring ATIV/EnCAP adoption remains an audit/documentation deliverable in `docs/UPDATING.md`; those repositories were not modified.
