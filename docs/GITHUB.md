# GitHub repository settings

These settings require an authenticated repository administrator. They were **not** changed by the original repository-file standardization, when GitHub authentication was unavailable. The unsigned-history migration also leaves repository settings unchanged. Verify the live settings before marking any item complete.

## Main branch

Use a ruleset for `main` that requires outside contributors to use a pull request and pass the Native platforms checks relevant to the change. Do not require signed commits: EWAF uses unsigned commits with DCO sign-off. Verify that active branch rules and rulesets do not enforce commit signatures. Keep Thomas Lothian's administrator ability to merge or push directly when necessary. Do not create a second human approval gate solely for stable releases; the owner-created annotated tag is release authorization. A `v*` tag ruleset restricted to Thomas Lothian is recommended; CI already checks the triggering owner account. A stable tag must be annotated; CI checks its version and triggering owner account, without a tag-signature requirement.

## Security

Enable GitHub private vulnerability reporting and make it the primary route until public security email is set. `SECURITY.md` intentionally leaves that email as **TBD**.

## Labels and milestones

Baseline labels: `bug`, `enhancement`, `documentation`, `security`, `dependencies`, `accessibility`, `build`, `release`, `platform: macOS`, `platform: Windows`, `platform: Linux`. Keep repository-specific labels that remain useful. Create milestones for concrete scheduled releases only; no future release number is assumed here.

Issue forms and the pull request template are in `.github/`. Dependabot checks Cargo and GitHub Actions as individual weekly updates. Review each dependency change against the license audit, native support floor and full CI matrix.
