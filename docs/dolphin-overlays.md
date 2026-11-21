# Dolphin Icon Overlays

Implementation status: the overlay plugin, its client cache, the D-Bus transport, the daemon
provider, and the first icon set are implemented and covered by automated tests. **They have not
been verified inside a running Dolphin**: native repaint in every view, polling latency on real
trees, and network-mount behavior remain open Phase 3 gates (see
[overlay-status-design.md](overlay-status-design.md#verification-and-remaining-empirical-gates)).

## How it works

Dolphin loads `linuxgitshell_overlayicon.so` from `kf6/overlayicon` and calls
`KOverlayIconPlugin::getOverlays(url)` on its main thread while painting. The plugin therefore does
nothing but a bounded in-memory lookup:

1. `getOverlays()` accepts only local `file:` URLs, normalizes the path, and looks it up in the
   process-wide `OverlayClient` cache (4,096 keys, least recently used eviction). A fresh `Ready`
   entry returns its emblem name; anything else returns an empty list and queues a request.
2. Requests leave from the event loop, in batches of at most 256 paths and at most four at a time,
   to `org.linuxgitshell.Experimental.Overlays1` served by `linuxgitshell-daemon`. The first request
   activates the daemon by name; constructing the plugin or painting never does.
3. The daemon runs Git once per repository (status plus tracked-path inventory), not per icon, and
   pushes records back as unicast D-Bus signals. The client turns them into `overlaysChanged(url,
   icons)` so that views repaint or clear the item. Icons also disappear on their own when their
   30-second freshness lapses, so an idle view never keeps a stale emblem.
4. The client renews every retained key every 30 seconds (the daemon's lease is 90 seconds), retries
   failures with 1, 2, 4, … up to 30 second backoff, and clears everything when the daemon goes away
   or is replaced. Signals are accepted only from the bus-verified owner of the service name.

Nothing is shown for items outside a repository, inside bare or sparse-checkout repositories, under
`/proc`, `/sys`, `/dev`, `/run`, or on network or unclassified filesystems.

## Icons

| State | Emblem name | Shape |
| --- | --- | --- |
| Clean | `emblem-linuxgitshell-clean` | green circle, check mark |
| Modified (also renamed, type-changed) | `emblem-linuxgitshell-modified` | orange rounded square, pencil |
| Added (also copied) | `emblem-linuxgitshell-added` | blue circle, plus |
| Conflict | `emblem-linuxgitshell-conflict` | red triangle, exclamation mark |
| Untracked | `emblem-linuxgitshell-untracked` | dashed ring, dot |

Folders show the highest-ranking state below them (`Conflict` > `Modified` > `Added` > `Untracked`
> `Clean`). Deleted and ignored items are modeled but have no glyph yet, so they show nothing and
are never shown as clean. The icons are installed under `hicolor/scalable/emblems`; icon themes can
override them by name. See [licensing.md](licensing.md#icons).

## Configuration

Settings use KConfig in `~/.config/linuxgitshellrc`:

```ini
[Overlays]
Enabled=false
```

Overlays are on by default when the plugin is installed. The setting is read when Dolphin loads the
plugin, so restart Dolphin after changing it. A disabled start-up sends no request, starts no
timer, and never activates the daemon. Live reload and a settings interface belong to the Settings
GUI phase.

Folder propagation is a separate daemon-side setting, independent of `Enabled`:

```ini
[Overlays]
RecursiveFolders=false
```

It defaults to `true`: a folder shows the highest-ranking state below it. With `false`, ordinary
folders show nothing, while explicit untracked or ignored directories and submodule entries keep
their own state. The daemon reads it once at start-up, so stop the running daemon (for example
`pkill linuxgitshell-daemon`) and let the next overlay request activate a new one.

## Trying it

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build -j && cmake --install build
kquitapp6 dolphin; dolphin /path/to/a/repository
```

Debug logging for the plugin and daemon: `QT_LOGGING_RULES="linuxgitshell.*.debug=true"` (add
`QT_FORCE_STDERR_LOGGING=1` when the journal would otherwise capture it). Logs contain counts,
durations, and error categories, never paths. Remove the plugin with `cmake --build build --target
uninstall` if your build generates it, or delete the installed `linuxgitshell_overlayicon.so`, the
`emblem-linuxgitshell-*.svg` files, and the two `org.linuxgitshell.Experimental.*.service` files.

## Automated coverage

| Test | Covers |
| --- | --- |
| `dolphin.overlay_client` | cache, deferred requests, batching, generations, revisions, freshness, backoff, eviction, disabling |
| `dolphin.overlay_client_dbus` | the real daemon by activation: real states, spoofed senders, service loss and recovery, absent service, release |
| `dolphin.overlay_plugin` / `_disabled` | the built module loaded like KIO does, 10,000 cold callbacks with no Git or bus work, disabled-by-settings run |
| `daemon.overlay_provider`, `daemon.overlay_dbus`, `daemon.overlay_settings` | provider, service, and settings side |
