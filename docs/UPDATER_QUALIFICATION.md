# Updater qualification record

Status on 2026-10-01: **not end-to-end qualified; no release tag authorized by the evidence below**. This record accompanies the `codex/updater-qualification` branch. Version remains 1.0.5. A qualification checkpoint is needed to execute native CI; it is not a claim that all tests have passed.

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
| Windows native tests | No local Windows runner/toolchain; CI pending |

Local artifact: `dist/EWAF-1.0.5-macos-arm64.zip`, containing `com.tlolabs.ewaf` 1.0.5 and embedded Sparkle. This is an ad-hoc signed development-channel artifact, **not a production artifact or VERSION A**. Manual native UI checks confirm that the disabled/unconfigured updater reports its status and leaves the app usable. These checks do not demonstrate discovery, download, installation or relaunch.

Logs are local ignored files under `build/qualification-*.log`; XCTest results are under `dist/UITests*.xcresult`. Formal scan artifacts are outside the repository under the Codex state directory. The scan service reported measured token usage of 7,037,302 total (7,007,542 input, 6,616,960 cached input), with complete usage coverage across four threads. These are service-reported totals, not estimates.

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
| Qualification commit / native CI | Pending below; no release commit declared qualified |
| Release tag / GitHub Release | None created |
| VERSION A / VERSION B | Not established |
| Production macOS signing / hardened runtime / nested signatures | Not qualified; local ad-hoc package only |
| Notarization / staple / Gatekeeper | Production checks not performed |
| GitHub updater discovery / authenticated download | Not performed on a published production release |
| Updater installation / relaunch / preserved settings and folders | Not performed |
| Post-update regression / same-version recheck | Not performed |

**VERSION A → GitHub discovery → authenticated download → installation → relaunch → VERSION B → passing regressions has not succeeded.** Missing production enrollment and unresolved native test results prevent declaring the macOS updater qualified. Windows and Linux also require native CI and signed older-to-newer installation evidence. Neighboring ATIV/EnCAP adoption remains an audit/documentation deliverable in `docs/UPDATING.md`; those repositories were not modified.
