# Code signing policy

Git commits and tags do not require cryptographic signatures. DCO
`Signed-off-by` trailers remain a contribution convention. Stable tags use
`vMAJOR.MINOR.PATCH` and must match the Cargo version. The
[unsigned-history migration](docs/history/UNSIGNED_COMMITS.md) records earlier
removal of signed commits.

GitHub Releases publish verified platform artifacts independently. A failure
on another platform or the internal Qt reference does not block them. Direct
downloads may be unsigned or unnotarized; platform-specific OS warnings and
trust prompts may apply. A package is never published unless its platform
build, package-integrity checks and functional smoke tests pass.

Developer ID signing, notarization, Azure Artifact Signing and Authenticode are
not prerequisites for development or direct-download publication. The macOS
package script retains optional Developer ID/notarization inputs, and Windows
packaging retains optional local signing support.

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
