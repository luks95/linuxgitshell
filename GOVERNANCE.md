# Governance

LinuxGitShell currently uses a maintainer-led model suitable for its early development phases. The
public repository is maintained through the [`luks95/linuxgitshell`](https://github.com/luks95/linuxgitshell)
project; additional maintainers and area owners will be documented here when appointed.

## Roles

- Contributors propose changes, report problems, review work, and participate in design discussions.
- Maintainers merge changes, manage releases, enforce project policies, and safeguard architectural
  and security boundaries.
- Area owners may be appointed for Git core, Dolphin integration, UI, packaging, translations, or
  releases when sustained contributors are available. Ownership does not grant unilateral control.

## Decisions

Routine changes are decided through review. Significant architectural or compatibility decisions
should be documented in an issue and, when accepted, an ADR under `docs/adr/`. The preferred method
is consensus; if consensus cannot be reached, active maintainers make and document the decision after
allowing a reasonable review period, normally seven days for non-urgent proposals.

Security fixes and restoration of a broken main branch may use an expedited private decision. The
reason and non-sensitive outcome should be documented afterward.

## Maintainer changes

A contributor may become a maintainer after sustained, constructive work, sound reviews, and clear
understanding of the project's safety and architecture rules. Existing uninvolved maintainers approve
the appointment by consensus. A maintainer may step down at any time or be removed for prolonged
inactivity, policy violations, or loss of trust through the same documented process.

The current repository owner, `@luks95`, is the initial maintainer. Repository recovery relies on
protected `main`, immutable release tags and artifacts, mandatory CI, and the documented release
process. Additional maintainers should receive the minimum forge permissions needed for their role.
