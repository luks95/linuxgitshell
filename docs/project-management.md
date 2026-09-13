# Project management

## Labels

The public GitHub repository currently provides these project labels:

| Label | Purpose |
| --- | --- |
| `bug` | Reproducible incorrect behavior |
| `feature` / `enhancement` | User-facing capability or enhancement |
| `design` | Architecture or compatibility proposal |
| `docs` / `documentation` | Documentation-only work |
| `good first issue` | Small task with clear guidance and acceptance criteria |
| `help wanted` | Maintainer-approved task open to community help |
| `security` | Non-sensitive tracking after private triage |
| `performance` | Measured responsiveness or resource concern |
| `accessibility` | Keyboard, assistive technology, contrast, or inclusive design |
| `packaging` | Installation, distribution, or release artifact work |
| `blocked` | Cannot progress until its documented dependency changes |

## Milestones

Milestones `v0.1.0` through `v1.0.0` exist using the version-to-phase mapping in
`roadmap-checklist.md`. The `v0.1.0` milestone is closed; `v0.2.0` is active. Versions are planning
targets, not promised dates.

## Triage

Maintainers should acknowledge new issues when capacity permits, confirm that reports contain no
secrets, reproduce bugs, assign the smallest useful milestone and labels, and request only information
needed to proceed. Security reports must leave the public tracker immediately and follow `SECURITY.md`.

Close duplicates with a link to the canonical issue. Close invalid or unsupported requests with a
specific explanation. Inactivity alone is not a reason for automatic closure; a stale issue may be
closed only after its current relevance is reviewed and a contributor is told how to reopen it.

## Repository configuration

The repository is public with protected `main`, required pull-request CI, force-push protection,
dependency and secret alerts, private vulnerability reporting, Discussions, and roadmap milestones.
Keep these controls enabled and keep the maintainer and area-owner list in `GOVERNANCE.md` current.
