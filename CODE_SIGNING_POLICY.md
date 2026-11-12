# Code signing policy

Git commits and tags do not require cryptographic signatures. DCO
`Signed-off-by` trailers remain a contribution convention. Stable tags use
`vMAJOR.MINOR.PATCH` and must match the Cargo version. The
[unsigned-history migration](docs/history/UNSIGNED_COMMITS.md) records earlier
removal of signed commits.

GitHub Releases publish verified platform artifacts independently. A failure
on another platform or the internal Qt reference does not block them. Local distributable macOS packages are signed and notarized by default. Explicit
dev/test packages and other-platform direct downloads may be unsigned; OS
trust prompts may apply. A package is never published unless its platform
build, package-integrity checks and functional smoke tests pass.

Developer ID signing and notarization are required by default for local macOS
distributable packages. Set SIGNING_IDENTITY and NOTARY_PROFILE; missing
prerequisites fail before compilation. Explicit --unsigned packaging and
ordinary build_and_run.sh development remain credential-free. Windows
Azure Artifact Signing and Authenticode policy remains unchanged.

Authenticated automatic updates have a separate, strict trust boundary.
macOS Sparkle feed/archive signatures, the EWAF-specific Ed25519 update
manifest and artifact-digest checks, and Windows Authenticode publisher/MSI
identity checks remain mandatory whenever those update paths are used. Missing
trust must fail closed; unsigned direct-download packages must never enter the
authenticated update feed. A checksum or build attestation is not a
replacement for update authentication. Do not modify operating-system
security mechanisms or weaken runtime verification.

`updates/trust.json` is currently unenrolled. The updater pipeline therefore
cannot publish authenticated updates. The historical Windows distribution
concern in `docs/LICENSE_AUDIT.md` was resolved by replacing the retired WinUI
application with the shared Qt presentation; it is not a current publication
gate. Signing credentials, when used, remain in Keychain or protected CI
secrets.
