# Governance

LinuxGitShell currently uses a maintainer-led model suitable for its bootstrap phase.

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

The project will publish the initial maintainer list and recovery procedure when it moves to a public
forge.
