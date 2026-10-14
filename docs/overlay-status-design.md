# Icon Overlay Status Design

Status: revised proposal for Phase 3 (`v0.2.0`, part 2), 2026-10-13. Nothing described here is
implemented yet. Builds on [repository-context-cache.md](repository-context-cache.md) and the
experimental context service. The model contract below is ready for an isolated implementation;
native repaint and performance checks remain required before shipping the plugin.

## Problem and constraints

Dolphin asks a `KOverlayIconPlugin` (KIOCore) for icon names synchronously:

```cpp
virtual QStringList getOverlays(const QUrl &item) = 0;
Q_SIGNALS: void overlaysChanged(const QUrl &url, const QStringList &overlays);
```

Treat `getOverlays()` as a main-thread paint callback: no Git, filesystem discovery, or synchronous
IPC. Use in-memory lookups and defer requests to the event loop. Native verification must establish
that emitting `overlaysChanged` updates existing icons in every supported view; correctness must
not depend on Dolphin calling `getOverlays()` again after that signal.

Git collection is per repository, never per icon. A refresh combines one status command and one
tracked-path inventory; a status result alone cannot prove that an omitted path is tracked and clean.

## Proposed boundary

| Piece | Location | Responsibility |
| --- | --- | --- |
| `RepositoryStatusIndex` | `libs/repositorycontext/` | Pure bounded model: tracked membership, explicit states, directory summaries, and snapshot validity. Qt Core only; no Git, filesystem, IPC, or widgets. |
| Status provider | `daemon/` | Discover repositories, collect Git data, translate it into model input, schedule refreshes, and publish requested paths. |
| Overlay client/plugin | `integrations/dolphin/overlays/` | Bounded process-wide cache, deferred asynchronous requests, freshness timers, and icon notifications. |

The provider translates `GitStatusEntry` into model-owned records. The model must not introduce a
GitCore dependency into RepositoryContext. Constructors and synchronous callbacks perform no D-Bus
work. Owner discovery, subscriptions, and requests run asynchronously from the event loop.

## Snapshot and state model

Each repository snapshot carries its identity, provider revision, monotonic capture time, validity,
tracked relative paths, and explicit entries preserving separate index/work-tree states, submodule
flags, and original/destination rename paths. Validity is separate from the presentation state:

- `Ready`: both collectors completed successfully and all model bounds were respected.
- `Pending`: initial collection or an explicit invalidation is awaiting a refresh.
- `Stale`: freshness elapsed without a replacement.
- `Degraded`: timeout, collector/parse failure, unsupported coverage, or a resource limit.
- `Unavailable`: outside a repository, bare repository, excluded location, or disappeared repository.

Only a fresh `Ready` snapshot may produce a state icon. All other validity values return `Unknown`
and remove any previous icon. A partial index is never published as complete. `Unknown` expresses
lack of evidence and is not a rank below `Clean` that aggregation can silently discard.

### File classification

For paths belonging to this repository, apply these rules in order:

1. An explicit status entry determines its state, including deletion or conflict.
2. A tracked path in the complete inventory with no explicit change is `Clean` for that snapshot.
3. An explicit ignored directory covers descendants, except tracked paths or more specific entries.
4. A path with neither tracked membership nor an explicit classification is `Unknown`.

The daemon resolves repository membership; the index never infers that every lexical descendant
belongs to the same repository. Nested repositories and submodule contents use their own indexes;
the parent index describes only the submodule entry. Excluded nested roots must be supplied as
ownership boundaries in model input; a lookup below them is Unknown in the parent index, even if
the parent status reports their directory. Relative keys reject absolute paths and `..`
escapes, retain case and unusual characters, and compare components (`dir/` must not match `dir2/`).
An empty relative key denotes the repository root. `.git` administrative paths have no overlay.

Known combined states use this presentation precedence:

`Conflict` > `Modified` > `Added` > `Deleted` > `Untracked` > `Ignored` > `Clean`

- Renamed and type-changed entries map to `Modified`; copied entries map to `Added`, at the
  destination. Preserve the original path for diagnostics and old-parent invalidation.
- Submodule commit or tracked-content changes map to `Modified`; untracked-only content maps to
  `Untracked`, never `Clean`. A conflict wins over all submodule flags.
- `Deleted` and `Ignored` exist in the model from the start. Until their icons ship, they produce
  an empty icon list, including removal of any previous icon; they never fall back to `Clean`.
- Keep detailed states in the snapshot even when the icon resolves to only one presentation state.

### Directories

Recursive propagation defaults to **on**, configurable independently of overlay enablement.
Untracked descendants participate by default; ignored descendants do not. A known ignored directory
itself is `Ignored`. With recursion enabled, a known directory shows the highest included descendant
state. With recursion disabled, ordinary directory summaries produce no icon (`Unknown`); explicit
untracked/ignored directories and submodule entries still have their own state.

Derive directories from tracked and explicit paths once per snapshot, in O(total path depth), with
bounded ancestor storage. A directory may be `Clean` only with a complete, fresh snapshot and no
included dirty descendant. An empty newly initialized repository root is `Clean` after successful
complete collection. Unknown arbitrary directories are not inferred clean. For rename transitions,
recompute both source and destination ancestry, including removal of old summaries.

## Git collection and limits

Through the reusable Git process runner, outside Dolphin, collect:

```text
git --no-optional-locks -C <root> ls-files --cached -z
git --no-optional-locks -C <root> status --porcelain=v2 -z --branch --show-stash --untracked-files=all --ignore-submodules=none --renames --ignored=matching
```

The inventory collector is new work for the provider increment. The existing `GitStatusReader`
already forces all untracked paths and can request ignored entries. The overlay provider will set
`includeIgnored=true` even when the ignored icon is disabled: classification and icon visibility are
separate. For Phase 3, overlays deliberately use all untracked paths regardless of
`status.showUntrackedFiles`; matching the CLI's hiding preference is deferred until coverage can be
represented without reporting omitted entries as clean. No repository configuration is changed.
Deduplicate inventory paths (unmerged index stages can repeat a name). Sparse-checkout coverage
is outside this MVP: detect it in the provider and publish `Unavailable` until its coverage rules
are implemented. The pure model accepts coverage metadata and never reads configuration itself.

Inventory and status are observations, not an atomic Git transaction. Publish only after both
succeed for the same provider refresh revision. Invalidation during collection discards its result
and schedules another refresh. External changes between commands are reconciled by the next poll;
Phase 3 does not promise instantaneous or transactional filesystem state.

Initial limits are implementation defaults to validate, not measured performance claims:

| Resource | Initial limit |
| --- | --- |
| Concurrent repository refreshes | 2; never overlap refreshes of one repository |
| Whole collection deadline | 10 seconds, covering both commands |
| Captured stdout + stderr | 16 MiB per command, enforced while reading |
| Model snapshot | 250,000 records including derived directories; 64 MiB accounted data |
| Provider indexes | 128 MiB accounted data total, LRU eviction |
| Observed repositories | 64 |
| Client path cache / service observations | 4,096 keys per client / 4,096 caller-path pairs globally |
| Request / notification batch | 256 paths / records; 1 MiB encoded message ceiling |
| Pending service paths | 1,024 globally |

Count path storage and record overhead conservatively; measure actual RSS separately. The current
process runner accumulates output without a cap: bounded capture and a typed overflow result are
prerequisites for the provider, not existing guarantees. On timeout, parse failure, or overflow,
cancel collection, discard partial data, and publish `Degraded`/`Unknown` for affected observations.
Do not claim even repository-level cleanliness from truncated output. Index eviction invalidates
its observers before dropping the data. Retries use bounded backoff; no tight error loop.

Normal, linked-worktree, and submodule repositories are eligible. Bare repositories have no
working-tree overlays. Exclude `/proc`, `/sys`, `/dev`, and `/run` with component-aware checks of
resolved paths in the daemon. Mounted network filesystems are excluded for this MVP; a `file:` URL
alone does not prove a local filesystem. If mount classification cannot be established, return
`Unavailable`. Local external drives remain eligible and require native verification.

## Observation, freshness, and refresh

Only explicit overlay requests create observations; context-menu discovery alone must not start
status polling. Observations are owned by the caller's unique session-bus name and requested path.

- Lease duration: 90 seconds; clients renew retained keys every 30 seconds in bounded batches.
- Snapshot freshness: 30 seconds from completed collection, using daemon monotonic time.
- Poll observed repositories every 15 seconds; coalesce triggers with a 250 ms debounce and one
  pending follow-up refresh per repository. These intervals are provisional performance defaults.
- A request always receives a current answer, including when no state changed. A fresh cached
  snapshot avoids Git. A cold/stale answer is `Pending`/`Unknown`, followed by the collection result.
- `RefreshRepository` invalidates current answers immediately and queues a debounced refresh.
  Application integration is part of the provider increment, not an existing method.
- Release on client LRU eviction, disablement, or bus disconnect. Expire leases even if a release is
  lost. Stop polling when the last observation disappears; cancel unneeded queued work.

The public plugin callback does not establish when a view stops displaying an item. Phase 3 therefore
renews the bounded set of keys retained by each live client until eviction, disablement, or process
exit; it does not claim to track visible views exactly. It may poll recently visited repositories.
Measure this cost with multiple windows before release. Watchers and more precise observation
tracking belong to Phase 4.

Freshness is checked on lookup and by a client timer, so an idle view also loses an expired icon.
A refresh failure, exclusion, disappearance, eviction, or daemon restart clears affected icons even
when there is no replacement state. A successful poll renews freshness for every observed key,
including unchanged states. Emit host repaint notifications only when the rendered icon changes.

## D-Bus contract (experimental)

Proposed separate interface and bus name: `org.linuxgitshell.Experimental.Overlays1`, object
`/org/linuxgitshell/Overlays`, served by the same daemon. Its activation entry and introspection XML
will be added with implementation; neither exists now. Keep Context1 unchanged. Follow the
[compatibility policy](dbus-api-policy.md); this is not the stable `Daemon1` API.

- `RequestOverlays(as paths, t requestId) -> (t generation, as acceptedPaths)` — validate and queue
  without waiting for Git. `requestId` is nonzero and unique among the caller's outstanding requests.
- `OverlayResults(t requestId, aa{sv} records, t generation)` — directed initial replies to the
  caller, possibly batched; exactly one initial record per accepted path, even on a cache hit or
  unchanged state. Send after the method reply. The accepted-path list determines completion.
- `OverlaysChanged(aa{sv} records, t generation)` — directed updates to observers, including
  invalidation and freshness renewal after a successful poll, even for unchanged presentation.
- `ReleaseOverlays(as paths) -> ()` — remove only the caller's observations; bounded like requests.
- `RefreshRepository(s root) -> b accepted` — accept only known repositories observed by the caller;
  invalidate and queue, never wait for Git in the method handler.

Every record requires `path` (s, original lexical request key), `repositoryRoot` (s, empty when
unknown), `state` (s), `validity` (s), `revision` (t), and `validForMs` (u). Revision increases for
every publication, including validity changes, within one daemon generation. Non-Ready records
use `Unknown` and zero validity time. Ready records contain only remaining freshness, never a new
full TTL for an old snapshot. Model or settings changes invalidate and republish affected keys.

Paths use the context service's lexical normalization and validation: absolute, nonempty, at most
4,096 QString code units. Split batches further to respect the encoded message ceiling, including
long multibyte paths. Return accepted keys deduplicated; malformed and over-capacity requests
must not leave clients waiting for an answer that will never arrive. An excessive request gets a
named `InvalidRequest` error; partial capacity acceptance is explicit in `acceptedPaths`. An empty
accepted list means no work was queued. Clients retry rejected keys with bounded backoff.

Initial Pending records complete request accounting but retain observations for later updates.
Allow 5 seconds for a method reply or accepted initial records; on transport failure/timeout, clear
in-flight accounting, remove stale presentation, and retry with backoff (1, 2, 4, up to 30 seconds).
Collection has its separate 10-second deadline and returns a typed validity change to observers.

Authenticate signal senders against the asynchronously tracked current service owner; a different
random generation alone is not proof of a restart. Owner loss clears all cached icons and pending
requests. On a new owner, obtain its generation from a fresh method reply, discard older messages,
and re-request retained keys. Within that generation, ignore lower revisions per path. Queue only
bounded early signals while establishing the owner/generation; never perform synchronous owner
resolution. Unknown optional fields are ignored; malformed required fields, unknown states, or
unknown validity values remove the affected icon and never preserve a previous `Clean` assertion.

Clients subtract elapsed request time from initial TTLs and age updates from monotonic receipt time.
This bounds normal cache use but cannot eliminate transport delay; freshness is best effort, not
an atomic guarantee. Generation, owner, request, and revision checks prevent replay of stale work.
Directed signals avoid broadcasting paths to unrelated ordinary subscribers; they are not a
confidentiality boundary against privileged session-bus monitoring. Logs contain counts, durations,
and error categories, never paths, repository contents, or Git output by default.

## Plugin behavior and settings

1. `getOverlays(url)` rejects non-local URLs, normalizes the key, and looks up the bounded cache.
   A fresh Ready entry returns its icon names. Otherwise return an empty list and queue a request
   with a zero-delay timer, deduplicating queued and in-flight keys.
2. Initial replies and updates refresh the cache. Emit `overlaysChanged(url, icons)` on changes,
   including an empty list for expiry or invalidation. Cache eviction also clears displayed icons.
3. Daemon absence never blocks Dolphin. Renew retained observations asynchronously, in batches,
   respecting backoff and lease expiry. No constructor or paint callback may activate the daemon.
4. Overlay enablement defaults to on when installed; recursion defaults to on. Implement the
   toggles through KConfig and document configuration/reload instructions in the plugin increment.
   The full Settings GUI remains Phase 14. On disable, clear existing icons, stop timers, release
   observations, and unsubscribe. A disabled startup performs no requests or background work.

## Icons

Create freely licensed icons for clean, modified, added, conflict, and untracked, legible at 16,
22, and 32 px, HiDPI, and light/dark themes. Use distinct shapes and project-specific `emblem-*`
icon names that themes can override. Record source, license, and attribution in `docs/licensing.md`.
Add `icons/` only with its first real asset. Deleted/ignored glyphs follow after the base MVP.

## Verification and remaining empirical gates

The following design choices replace the earlier open questions: polling owns Phase 3 freshness;
recursive propagation starts enabled; untracked classification is complete independently of Git's
CLI display preference. Timer values and capacity limits above remain subject to measurement.

Automated acceptance for the first increment (`RepositoryStatusIndex`):

- Construct model input directly; no Git execution, filesystem inspection, IPC, or GitCore linkage.
- Exercise known clean membership, arbitrary missing paths, ignored prefixes with tracked exceptions,
  untracked entries, all combined-state precedence, conflicts, and submodule flags.
- Cover destination/source rename ancestry, recursive on/off, ignored/untracked aggregation options,
  nested-repository ownership boundaries, component prefixes, unusual paths, and invalid relative keys.
- Cover atomic snapshot replacement, stale/non-Ready lookups, monotonic revisions, bounds/eviction,
  and complete clearing of old states; partial data and Unknown never become Clean.
- Supply a deterministic clock/time value and configurable small limits so tests need no sleep.

Provider and transport acceptance:

- Temporary repositories: clean, modified, staged, untracked, ignored, renamed, conflicted, bare,
  linked worktrees, submodules, nested repos, empty repos, missing paths, and special filenames.
- Ignored icons disabled and `status.showUntrackedFiles=no` still cannot misclassify paths as clean.
- Output overflow during collection, deadline, parser error, concurrent mutation/invalidation,
  deduplicated refresh, directory invalidation, and global resource limits.
- Private D-Bus: first and second client initial replies on an unchanged snapshot; partial acceptance;
  Pending-to-Ready transition; TTL renewal/expiry; directed notifications; unknown wire values;
  reordered/late replies; sender validation; owner change; crash; lease release/expiry; slow service.
- A real-module test building overlays for 10,000 items performs no Git or D-Bus call before
  returning to the event loop, and disablement stops all recurring work.

Native gates before closing Phase 3:

- Five MVP states and clearing repaint correctly in icons, details, and compact views, including
  folder scaling, idle views, daemon restart, settings changes, and two Dolphin windows.
- Edit, create, delete, stage, rename, and resolve become visible within the measured polling bound
  without restarting Dolphin. Verify renewal/expiry without relying on repeated paint callbacks.
- Record p50/p95/max callback time, refresh latency, daemon RSS, and retained-observation cost for
  deep trees, a 100k-file fixture, and a local external mount; verify network-mount exclusion.
- Benchmark status and inventory with/without existing untracked-cache and fsmonitor on disposable
  fixtures. Do not change contributor repositories. Tune defaults from evidence before `v0.2.0`.

## Proposed increments

1. `RepositoryStatusIndex` model and deterministic unit tests against the contract above.
2. Bounded process capture, inventory collector, daemon provider, experimental XML/activation,
   application refresh hook, and provider/D-Bus integration tests.
3. Overlay plugin, client cache, KConfig toggles, and first icon set with real-module tests.
4. Native verification, measured defaults, install/removal documentation, and `v0.2.0`.
