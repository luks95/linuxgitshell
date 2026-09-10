# Project management

## Labels

The public forge should begin with these labels:

| Label | Purpose |
| --- | --- |
| `bug` | Reproducible incorrect behavior |
| `feature` | User-facing capability or enhancement |
| `design` | Architecture or compatibility proposal |
| `docs` | Documentation-only work |
| `good first issue` | Small task with clear guidance and acceptance criteria |
| `help wanted` | Maintainer-approved task open to community help |
| `security` | Non-sensitive tracking after private triage |
| `performance` | Measured responsiveness or resource concern |
| `accessibility` | Keyboard, assistive technology, contrast, or inclusive design |
| `packaging` | Installation, distribution, or release artifact work |
| `blocked` | Cannot progress until its documented dependency changes |

## Milestones

Create forge milestones for `v0.1.0` through `v1.0.0` using the version-to-phase mapping in
`roadmap-checklist.md`. Versions are planning targets, not promised dates.

## Triage

Maintainers should acknowledge new issues when capacity permits, confirm that reports contain no
secrets, reproduce bugs, assign the smallest useful milestone and labels, and request only information
needed to proceed. Security reports must leave the public tracker immediately and follow `SECURITY.md`.

Close duplicates with a link to the canonical issue. Close invalid or unsupported requests with a
specific explanation. Inactivity alone is not a reason for automatic closure; a stale issue may be
closed only after its current relevance is reviewed and a contributor is told how to reopen it.

## Repository configuration

After publication, require pull requests and passing CI on the default branch, prohibit force-push,
enable dependency/security alerts, publish the support channel, and link the roadmap milestones. Keep
the maintainer and area-owner list in `GOVERNANCE.md` current.
