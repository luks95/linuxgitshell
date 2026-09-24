# Repository Context Cache Design

Status: accepted for Phase 2; designed in
[`#10`](https://github.com/luks95/linuxgitshell/issues/10). The snapshot model and bounded cache
live in `libs/repositorycontext/`, and the experimental context service in `daemon/` (see
[Implemented service](#implemented-service)), and the plugin client in
`integrations/dolphin/contextmenu/` (see [Implemented client](#implemented-client)). Repository-aware
actions are tracked by [`#12`](https://github.com/luks95/linuxgitshell/issues/12).

## Problem and constraints

Dolphin calls `KAbstractFileItemActionPlugin::actions()` synchronously while constructing a context
menu. A Git command, recursive scan, network access, slow filesystem lookup, or blocking D-Bus call
in that method can freeze Dolphin. Repository-aware actions therefore need a local snapshot that is
already available when the menu opens.

The repository identity cannot be inferred by assuming that `.git` is always a directory:

- normal repositories normally have a `.git` directory;
- linked worktrees and submodules normally have a `.git` file pointing elsewhere;
- bare repositories have no normal working tree or nested `.git` marker;
- symlinks, separate Git directories, mount points, and deleted or moved paths complicate identity.

The existing asynchronous `RepositoryDiscovery` and Git's `rev-parse` output remain the source of
truth. The Dolphin plugin must not parse `.git` files or execute Git itself.

## Proposed boundary

An external session service owns repository discovery and the authoritative cache. The implementation
may begin as a focused context service, but its models and protocol must be reusable by the Phase 4
daemon and versioned D-Bus API.

The plugin owns only a bounded in-process snapshot client:

1. `actions()` validates selection count and local URLs using data already supplied by KIO.
2. It performs an in-memory lookup without filesystem or IPC access.
3. A warm entry produces only actions valid for that snapshot.
4. A cold or stale entry produces the safe generic `Open with LinuxGitShell` action and schedules an
   asynchronous lookup after `actions()` returns.
5. The service runs `RepositoryDiscovery` outside Dolphin and replies asynchronously.
6. The client stores the snapshot; a later menu opening can use repository-aware actions.

No synchronous D-Bus fallback is allowed, even with a short timeout. Service absence, restart, or a
cold cache must degrade to the generic action without blocking or crashing Dolphin.

## Snapshot model

A repository-context snapshot should contain only the fields required to decide menu availability:

- normalized requested-path key and repository membership;
- repository root and type: normal, bare, linked worktree, or submodule;
- whether the selected path is the repository root;
- operations in progress;
- whether a remote and upstream exist;
- generation and monotonic freshness timestamps;
- a typed unavailable, outside-repository, or discovery-error state.

The original selected path remains the argument passed to an external application. Normalized and
canonical paths are cache keys, not replacements for the user's selection.

For multiple selections, repository-specific actions appear only when every item has a warm snapshot
and all snapshots identify the same compatible repository. Otherwise the plugin returns only a safe
fallback or no action according to the selection policy and starts asynchronous refreshes.

## Bounds and invalidation

Initial implementation targets, subject to measurement, are:

- at most 512 repository records and 4,096 path aliases per user session;
- least-recently-used eviction after the bounds are reached;
- five-minute freshness for positive path-to-repository mappings;
- 30-second freshness for outside-repository and transient-error entries;
- immediate invalidation after a LinuxGitShell operation changes known repository identity;
- invalidation after watched Git metadata or relevant parent paths move, disappear, or change;
- full client snapshot invalidation when the service generation changes after restart.

`.git` directory or file changes are useful invalidation signals, but never the only repository
identity source. Bare repositories and indirection through `.git` files must always be resolved by
the external Git-backed service.

## Privacy and diagnostics

Cache and service logs report counts, durations, generations, result types, and error categories.
They do not log selected paths, repository contents, remotes, Git output, or environment variables by
default. Opt-in diagnostics must reuse the existing sanitization rules.

## Performance target

The synchronous `actions()` path should have a local p95 below 2 ms for both warm and cold snapshots
on the supported workstation. It must perform no application-directed filesystem or IPC wait. CI
will test behavior deterministically; native measurements will record p50, p95, and maximum latency
before repository-aware actions are enabled by default.

## Verification plan

Automated coverage must include:

- normal repositories with a `.git` directory;
- linked worktrees and submodules with a `.git` file;
- bare and outside-repository paths;
- files, directories, symlinks, spaces, Unicode, and newlines;
- warm, cold, expired, evicted, invalidated, and service-restart snapshots;
- mixed and same-repository multiple selections;
- unavailable or slow service behavior without blocking the plugin;
- a test proving `actions()` performs no Git or filesystem discovery.

Native Dolphin verification must cover the same visible policies and confirm responsiveness on local,
large, external, and deliberately slow locations before the Phase 2 exit criteria are marked complete.

## Implemented service

`linuxgitshell-daemon` is the first increment of the Phase 4 session daemon. It registers the
experimental name `org.linuxgitshell.Experimental.Context1` with object
`/org/linuxgitshell/Context`, as specified in
[`dbus/org.linuxgitshell.Experimental.Context1.xml`](../dbus/org.linuxgitshell.Experimental.Context1.xml).
Following [`dbus-api-policy.md`](dbus-api-policy.md), it does not use the stable `Daemon1` name and
may change without a compatibility period.

- `RequestContext(as paths) -> t generation` returns immediately. Every accepted path is answered
  later by `ContextReady(s path, a{sv} context, t generation)`, keyed by the lexical path key.
- The generation is random, non-zero, and new on every start, so clients can drop snapshots after a
  restart.
- Up to 256 paths per request and 1,024 queued paths are accepted; relative, empty, and
  longer-than-4,096-character paths are ignored. Duplicate pending paths are resolved once.
- Two discoveries run concurrently, each limited to 10 seconds. A timeout, a missing path, or a
  Git failure produces a short-lived `error` state; only paths outside a repository produce
  `outside`.
- The service keeps its own `RepositoryContextCache`, so repeated requests and several Dolphin
  windows do not rerun Git while an entry is fresh.
- The installed D-Bus activation file starts the service on the first asynchronous request. A
  second instance exits because the name is already owned.
- Logs report counts, durations, and error categories, never paths. `ContextReady` is a broadcast
  signal, so path keys are visible to other clients of the user's own session bus; they never leave
  it.
- A repository root whose path contains a newline is reported as `error`, because Git reports
  repository paths one per line. Files with newlines inside a repository are supported.

## Implemented client

`RepositoryContextClient` is a single process-wide object owned by the application, because Dolphin
may create a new plugin object for each context menu. Constructing it performs no D-Bus work.

1. `actions()` computes the lexical path key and calls `lookup()`, a hash lookup in a
   `RepositoryContextCache` with the default bounds.
2. A cold or stale key goes through `refreshLater()`, which only appends to an in-memory queue and
   schedules a zero-delay timer. Keys already queued or in flight are skipped; at most 256 keys are
   in flight, and an unanswered key can be requested again after 30 seconds.
3. From the event loop, `DBusRepositoryContextTransport` sends `RequestContext` with
   `QDBusConnection::asyncCall()`, which also triggers D-Bus activation. It uses no generated proxy,
   because proxies resolve the name owner synchronously. It subscribes to `ContextReady` without a
   sender filter so that subscribing needs no name lookup either.
4. The returned generation, and the generation of each reply, replaces the cache generation when it
   differs, which drops snapshots from a previous service instance. Unrecognized replies are ignored.
5. A failed or refused call releases its keys, so a later menu can retry.

The menu itself is unchanged in this increment: one local selection still produces only
`Open with LinuxGitShell`.

The plugin test loads the real module on a private bus with an activatable service. It builds 200
menus and verifies three things. The service is not activated before control returns to the event
loop, so no synchronous IPC happened. A `git` placed first in `PATH` is never executed. The service
is then activated by the deferred request. Temporarily adding either a synchronous D-Bus call or an
in-process Git execution to `actions()` makes the test fail.

Offscreen measurements in a Debug build on the development workstation gave p50 0.015 ms, p95
0.03 ms, and max 1.8 ms for `actions()`; the maximum is the first call. CI enforces only a loose 20 ms
p95 bound to catch blocking work. Native Dolphin measurements remain required before
repository-aware actions are enabled by default.
