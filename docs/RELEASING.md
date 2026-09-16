# Building and releasing E.W.A.F.

## Development builds

`script/package.sh` builds the SwiftPM executable, stages `dist/EWAF.app`, writes
bundle metadata, signs it, verifies its signature, and creates
`dist/EWAF-macOS.zip` and its SHA-256 checksum. The native identifier is
`com.tlolabs.ewaf`; the Python app had no bundle identifier or custom icon.

```sh
UNIVERSAL=1 APP_VERSION=1.0.1 APP_BUILD=2 ./script/package.sh
```

`UNIVERSAL=1` builds Apple silicon and Intel slices. Without it the build targets
the current machine. macOS 14 is the minimum supported OS. The default package
configuration is release; the Run script chooses debug.

Without `SIGNING_IDENTITY`, the app is ad-hoc signed for development. This does
not establish Developer ID trust and is not notarization. Public distribution
should wait for signed/notarized artifacts. Do not disable Gatekeeper globally.

## Local signing and notarization

Install the owner's **Developer ID Application** certificate and private key in
Keychain. No valid identities were installed during the rewrite. Do not commit
certificates, private keys, passwords, or exported Keychains.

Store notarization credentials interactively:

```sh
xcrun notarytool store-credentials EWAF-Notary
SIGNING_IDENTITY='Developer ID Application: Your Name (TEAMID)' \
NOTARY_PROFILE=EWAF-Notary UNIVERSAL=1 ./script/package.sh
```

The packaging script enables hardened runtime when Developer ID signing is
configured, waits for notarization, staples and validates the ticket, then
rebuilds the ZIP and checksum. Verify a release with `codesign --verify --strict`
and `spctl --assess --type execute --verbose dist/EWAF.app` on a clean Mac.

## GitHub Actions

- `Tests`: preserves the Python 3.10/3.14 regression matrix.
- `Native macOS`: Swift tests, generated project drift check, Xcode UI tests,
  universal package, and downloadable development artifacts on pushes to main.
- `v*` tags: verification must pass before release packaging. The workflow creates
  a **draft** GitHub release containing the ZIP and checksum for owner review.
- Test results are uploaded even on failure. UI tests need a logged-in graphical
  macOS session; hosted macOS runners provide one.

Optional GitHub repository secrets for signed releases:

| Secret | Purpose |
| --- | --- |
| `MACOS_CERTIFICATE_P12_BASE64` | Base64 Developer ID certificate/private key export |
| `MACOS_CERTIFICATE_PASSWORD` | Password protecting that export |
| `MACOS_KEYCHAIN_PASSWORD` | Random password for the temporary CI Keychain |
| `MACOS_SIGNING_IDENTITY` | Full Developer ID Application identity |
| `NOTARY_APPLE_ID` | Apple account for notarization |
| `NOTARY_APP_PASSWORD` | App-specific password |
| `APPLE_TEAM_ID` | Developer team identifier |

CI imports the certificate into a temporary Keychain and removes it afterward.
No paid service is introduced. A Developer Program membership, if needed, must
be supplied/approved by the owner. Unsigned tag builds remain draft development
artifacts; review their signing status before publishing.

## Acceptance and releases

1. Run automated tests and review the manual acceptance checklist.
2. Owner manually accepts the native replacement. Until then, keep all Python
   baseline files available, including `legacy-python/`.
3. Configure signing/notarization credentials before public distribution.
4. Tag the accepted version (for example `v1.0.1`) and push the tag.
5. Inspect the draft release, validate the downloaded build on a clean machine,
   then publish the release manually.

This private repository currently distributes updates through authenticated
GitHub downloads. There was no legacy updater to preserve. An unattended updater
needs a defined authenticated distribution channel and update-signing keys; it
must not embed a GitHub token or reuse signing credentials from the developer's
machine. This remains an explicit distribution follow-up, not a claimed feature.
