# Contributing

Outside contributions are welcome. Thomas Lothian maintains EWAF. Pull requests are welcome but are not required for commits or merges; there are no required reviews or status checks. Issues are useful for discussing large behavior changes but are not a prerequisite. Read [BEHAVIOR.md](docs/BEHAVIOR.md) and [PARITY.md](docs/PARITY.md) before changing a workflow; include macOS, Windows and Linux impact in change documentation when applicable.

Run the applicable checks in [TESTING.md](docs/TESTING.md). Document which platforms you personally tested and which CI validated. Preserve existing identifiers and third-party notices. New code and assets must have clear provenance and be compatible with GPL-3.0-or-later. Do not apply EWAF's license headers to third-party material. The [distribution audit](docs/LICENSE_AUDIT.md) tracks package-specific terms separately.

EWAF uses the [Developer Certificate of Origin](https://developercertificate.org/). DCO sign-off is a contribution convention, not an enforced commit or merge gate. When practical, add `Signed-off-by: Name <email>` (for example, `git commit -s`). This certifies your right to contribute under the project's applicable terms. Use unsigned Git commits; cryptographic commit signing is not required or recommended. The DCO `Signed-off-by` trailer is plain commit-message text, not a PGP signature. Use concise, plain-English imperative commit subjects. A CLA is not required.

## Local Git setup

Disable automatic commit signing for this clone, including when your global Git settings enable it:

```sh
git config --local commit.gpgsign false
git config --local tag.gpgsign false
```

Use `git commit -s` for the DCO trailer; omit `-S` / `--gpg-sign`. Release tags require no cryptographic signature; platform artifact and update-manifest requirements follow [CODE_SIGNING_POLICY.md](CODE_SIGNING_POLICY.md). See the [unsigned-history migration](docs/history/UNSIGNED_COMMITS.md) for historical commit preservation and changed IDs.
