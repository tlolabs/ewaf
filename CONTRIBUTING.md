# Contributing

Outside contributions are welcome. Thomas Lothian maintains EWAF and has final merge and release authority. Discuss large behavior changes in an issue first. Read [BEHAVIOR.md](docs/BEHAVIOR.md) and [PARITY.md](docs/PARITY.md) before changing a workflow; include macOS, Windows and Linux impact in the pull request.

Run the applicable checks in [TESTING.md](docs/TESTING.md). Document which platforms you personally tested and which CI validated. Preserve existing identifiers and third-party notices. New code and assets must have clear provenance and be compatible with GPL-3.0-or-later. Do not apply EWAF's license headers to third-party material. The [distribution audit](docs/LICENSE_AUDIT.md) tracks package-specific terms separately.

EWAF uses the [Developer Certificate of Origin](https://developercertificate.org/). Sign off each contributed commit with `Signed-off-by: Name <email>` (for example, `git commit -s`). This certifies your right to contribute under the project's applicable terms. Use unsigned Git commits; cryptographic commit signing is not required or recommended. The DCO `Signed-off-by` trailer is plain commit-message text, not a PGP signature. Use concise, plain-English imperative commit subjects. A CLA is not required.

## Local Git setup

Disable automatic commit signing for this clone, including when your global Git settings enable it:

```sh
git config --local commit.gpgsign false
```

Use `git commit -s` for the DCO trailer; omit `-S` / `--gpg-sign`. Release-tag signing and platform artifact signing follow [CODE_SIGNING_POLICY.md](CODE_SIGNING_POLICY.md). See the [unsigned-history migration](docs/history/UNSIGNED_COMMITS.md) for historical commit preservation and changed IDs.
