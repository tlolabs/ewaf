# Unsigned Git commits

On 2026-09-30, the owner requested removal of PGP signatures from historical commits and unsigned commits going forward. The local migration removed three commit signatures and rewrote their descendants: seven commit IDs changed in total. Every original message (including DCO trailers), author, committer, timestamp, file tree and merge-parent relationship was preserved. Only signature headers and references to rewritten objects changed.

## Scope and publication

Local `main` and the annotated `v1.0.4` tag now reference unsigned history. The tag was already unsigned; its message and tagger metadata are preserved. `v1.0.3` and the three existing `codex/*` branches needed no changes. The legacy snapshot remains on `codex/archive-legacy-1.0.2`; no legacy files were restored to the active tree.

The owner subsequently authorized force-pushing all pending changes to GitHub. Publication includes rewritten `main`, the affected `v1.0.4` tag and six remote-only Dependabot branches discovered during the remote audit. Those six branches each contained one additional PGP-signed commit; their messages, metadata, dependency changes and source trees are preserved in unsigned replacements. Across the local and remote audit, nine signatures were removed and thirteen commit IDs changed. The development tag `dev-a6f1b98b3761` requires no change. Publication uses one atomic push with explicit leases against the observed remote object IDs. GitHub Releases, release assets and branch-protection settings are not edited by this migration.

After publication, existing clones must realign with the rewritten branch after saving local work. Do not use an ordinary pull to combine the original and rewritten histories. Dependabot can create signed commits again on future updates; this migration removes the signatures present at the time of the audit.

Existing release downloads, CI records and historical documentation can still identify original commit IDs. Rewriting Git objects does not rebuild, re-sign or revalidate published packages. Preserve those original provenance records; use the mapping below to relate them to the equivalent source trees.

## Preservation and verification

Before rewriting, a verified Git bundle of all branch, tag and remote-tracking history was saved locally at `.git/unsigned-history-backup-2026-09-30/original-history.bundle`. That directory also contains original refs, the original repository Git configuration, the original commit map and changed-ref map. A second verified bundle, `remote-history-before-publication.bundle`, preserves the subsequently fetched remote-only history. `full-commit-map.json`, `remote-refs-before-publication.json` and `dependabot-ref-updates.json` record the expanded mapping and publication inputs. These recovery files are untracked and are not part of a release. Original signed objects remain recoverable from the bundle and local reflogs.

Verification compared the raw replacement commits against their originals, allowing only signature removal and mapped parent IDs. All 39 initially reachable commits and six additional remote-only commits were examined; the active local branch/tag history contains no commit-signature headers. The working tree was unchanged by the rewrite. Repository-local `commit.gpgsign` is now `false`; contributors should apply the setup in [CONTRIBUTING.md](../../CONTRIBUTING.md) to each clone. Global Git configuration is unchanged.

| Original commit | Unsigned-history commit |
| --- | --- |
| `1d8ab09e1a59f70b1a56292223c47bdd948ede38` | `db2afdea64d1aa965d35c1ee6950f52b3a04d1af` |
| `3d7b48222a9efa5b73cf6039ada51e31af77e5ab` | `2f31bc57c99876dcbcc75795d80f0cc4c368960f` |
| `d9573adb2b594dacd434c5d9a5013f64cf363815` | `58dc1e2a0d3c1fc60a620a728493334001aee542` |
| `42ea235e0965c38a3066264bd8abcb19972edbbd` | `7cc21ac9568950cacb4442191e91f0d1c6454d5b` |
| `d1c27e85c9382d35f43fd61c37c9b18975ec17f9` | `f55b316de353c7a8382d36200e39deb7a57e8e3a` |
| `fda94d26e28b86149f55261deb5e2ea46cd87adb` | `728c87d4ea137342b7f48f84817ec7e141331fe5` |
| `9e018209e29eda26412d72f85603112fcdecd71b` | `ddf144ddddb423c3376007446639471cc5d83cae` |

Additional remote-only Dependabot commits:

| Original commit | Unsigned-history commit |
| --- | --- |
| `1b495b4001a96363a746a651f1bcf3c1f552b6d9` | `219a333bc3e8c5d7251b535fec3182b3d9b781cd` |
| `9a17a2b0f0037efb8df135d8cb4dd4627035e540` | `a53408a1c21fe46c02658969de7ee234fe65d991` |
| `dc316be2581ebc70083b63ca304ea80ce637130d` | `268addc1d84f5789014ef27d28728d294ab3d34b` |
| `e3de527c0831e73bf47cc6f6b61adae2585961ad` | `2a6602946103bab6843e7422482551db1d68cdec` |
| `55386578f2c643ec80f5afc553bf3a8823e09957` | `bce6eb0078799afca48e6d8f8b0b9d8135d35449` |
| `2e97e5c110d63424e071a5d5dfa0946f43189472` | `037fd4874904ed7e45f51b5b8d289fb46b505c08` |

## Platform impact

macOS, Windows and Linux application sources, native workflows, version numbers and package contents are unchanged. No runtime parity change or new native regression test is needed. The migration verifies Git object and source-tree preservation directly; it does not claim new native build, package or CI validation. Release tag creation, platform identity checks and signed update manifests are governed by [CODE_SIGNING_POLICY.md](../../CODE_SIGNING_POLICY.md).
