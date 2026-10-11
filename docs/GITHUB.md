# GitHub repository settings

## Nonblocking development and releases

EWAF has no required reviews, status checks, branch-protection rules or
repository rulesets. Read-only inspection on 2026-10-11 found that `main` was
not protected and the repository rulesets endpoint returned none. The
organization-level rulesets endpoint was unavailable to the inspecting
account, so inherited organization settings could not be verified.

Do not add required reviews, required checks, tag restrictions or manual
release approvals without explicit authorization from the repository owner.
Automated tests, package checks, attestations and security diagnostics remain
visible; they do not block unrelated development or successful platform
publication. See [CI/CD audit](CICD_AUDIT.md) and [release policy](RELEASING.md).

The native workflow publishes only packages that pass that platform's build,
integrity and functional smoke tests. Platforms publish independently. The
internal Qt reference is excluded. GitHub Releases can contain direct-download
packages without authenticated update metadata; that does not enable the
automatic updater or change its signature requirements.

## Security and dependencies

GitHub Actions is enabled and all action sources are allowed. Dependabot
security updates and vulnerability alerts are enabled. Dependabot checks
Cargo and GitHub Actions on its configured schedule. The current workflow has
no dedicated code-scanning job; findings from GitHub's security services
remain diagnostics and must not become merge or release gates.

Private vulnerability reporting is the primary route until a public security
email is configured. See [SECURITY.md](../SECURITY.md).

Issue forms and the pull request template are in `.github/`. Baseline labels
include `bug`, `enhancement`, `documentation`, `security`, `dependencies`,
`accessibility`, `build`, `release`, and per-platform labels. Create milestones
only for concrete scheduled releases; do not invent release versions.
