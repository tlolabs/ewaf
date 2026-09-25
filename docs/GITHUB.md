# GitHub repository settings

These settings require an authenticated repository administrator. They were **not** changed by the repository-file standardization because the available GitHub CLI token is invalid and the browser session is signed out. Verify the live settings before marking any item complete.

## Main branch

Use a ruleset for `main` that requires outside contributors to use a pull request and pass the Native platforms checks relevant to the change. Keep Thomas Lothian's administrator ability to merge or push directly when necessary. Do not create a second human approval gate solely for stable releases; the signed tag is release authorization. Protect `v*` tags so only Thomas Lothian can create official release tags. A stable tag must be annotated, cryptographically signed and verified by CI.

## Security

Enable GitHub private vulnerability reporting and make it the primary route until public security email is set. `SECURITY.md` intentionally leaves that email as **TBD**.

## Labels and milestones

Baseline labels: `bug`, `enhancement`, `documentation`, `security`, `dependencies`, `accessibility`, `build`, `release`, `platform: macOS`, `platform: Windows`, `platform: Linux`. Keep repository-specific labels that remain useful. Create milestones for concrete scheduled releases only; no future release number is assumed here.

Issue forms and the pull request template are in `.github/`. Dependabot checks Cargo, NuGet and GitHub Actions as individual weekly updates. Review each dependency change against the license audit, native support floor and full CI matrix.
