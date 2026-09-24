# Dolphin Context-Menu Integration

Implementation status: the minimal single-local-selection plugin is merged on `main` through
[PR #9](https://github.com/luks95/linuxgitshell/pull/9). Repository-aware menus are implemented under
[issue #12](https://github.com/luks95/linuxgitshell/issues/12) and still need native Dolphin verification.

This note records the KF6 extension API and the first implementation boundary for Phase 2. It was
validated on 2026-09-13 against Dolphin 26.08.0, KIO 6.29.0, the installed KF6 headers, and a current
KDE file-item action plugin.

## Extension mechanism

LinuxGitShell needs a dynamic file-item action plugin rather than a static service menu. Repository
membership, selection count, local versus remote URLs, and later repository state affect which
actions are valid. The public KF6 entry point is
[`KAbstractFileItemActionPlugin`](https://api.kde.org/kabstractfileitemactionplugin.html) from
`KF6::KIOWidgets`.

The plugin subclasses `KAbstractFileItemActionPlugin` and implements:

```cpp
QList<QAction *> actions(
    const KFileItemListProperties &fileItemInfos,
    QWidget *parentWidget) override;
```

`KFileItemListProperties` supplies the selected items, URLs, common MIME information, and whether
the complete selection is local. Returned actions must use `parentWidget` as their QObject/UI parent.
The `error(QString)` signal is available for errors that Dolphin should display.

The method is synchronous and runs while the host constructs its context menu. The installed KIO
6.29 header even records asynchronous, stoppable execution as future API work. LinuxGitShell must
therefore do no Git invocation, recursive scan, network access, or wait inside `actions()`.

## Registration and build

Use `K_PLUGIN_CLASS_WITH_JSON` with a JSON metadata file. A current KDE Connect implementation uses
the same factory macro, constructs its actions immediately, and performs later work asynchronously:

- [KDE Connect file-item action implementation](https://github.com/KDE/kdeconnect-kde/blob/master/fileitemactionplugin/sendfileitemaction.cpp)
- [KDE Connect plugin metadata](https://github.com/KDE/kdeconnect-kde/blob/master/fileitemactionplugin/kdeconnectsendfile.json)
- [KDE Connect plugin build](https://github.com/KDE/kdeconnect-kde/blob/master/fileitemactionplugin/CMakeLists.txt)

The standard KF6 CMake shape for this repository is:

```cmake
find_package(ECM 6.0 REQUIRED NO_MODULE)
list(PREPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
include(KDEInstallDirs6)
include(KDECMakeSettings)

find_package(KF6KIO 6.0 REQUIRED)

kcoreaddons_add_plugin(
    linuxgitshell_fileitemaction
    SOURCES GitActionPlugin.cpp
    INSTALL_NAMESPACE "kf6/kfileitemaction"
)
target_link_libraries(
    linuxgitshell_fileitemaction
    PRIVATE
        KF6::KIOWidgets
        KF6::I18n
)
```

The installed Qt plugin root is `/usr/lib/qt6/plugins`, and current system plugins are under
`kf6/kfileitemaction`. The KDE Connect build independently uses that same namespace. The KF6 API
page still contains two stale KF5 references in its prose/example, so those references must not be
copied into the implementation.

No Dolphin-private headers are required. Dolphin is a runtime and manual-test dependency; KIO
provides the public plugin interface.

## Validated dependency inventory

| Package | Version | State | Phase 2 role |
| --- | --- | --- | --- |
| `dolphin` | 26.08.0-5 | installed | Runtime host and manual testing |
| `kio` | 6.29.0-2 | installed | File-item action API and `KF6::KIOWidgets` |
| `kcoreaddons` | 6.29.0-1 | installed | Plugin factory and CMake plugin macro |
| `ki18n` | 6.29.0-1 | installed | Translatable action labels |
| `qt6-base` | 6.11.2-3 | installed | Qt Core and Widgets |
| `extra-cmake-modules` | 6.29.0-1 | installed | Standard KDE install paths and CMake settings |

`extra-cmake-modules` was added when implementation began. Its `KDEInstallDirs6` and
`KDECMakeSettings` modules provide `KDE_INSTALL_PLUGINDIR` and the library output directory expected
by `kcoreaddons_add_plugin`.

## Process boundary

The plugin decides its entries in `ContextMenuPolicy`, a UI-independent helper that only reads
memory-only snapshots, and launches every window as a separate process. The menu offers only actions
that the application can already perform:

| Selection | Snapshot | Entries |
| --- | --- | --- |
| Empty, remote, mixed, or more than 256 items | — | none |
| One local item | cold or stale | `Open with LinuxGitShell`, and an asynchronous refresh |
| One local item | inside a repository, including bare | `LinuxGitShell` ▸ `Show Status` for the selected path |
| One local item | outside a repository | none |
| One local item | discovery error or service unavailable | `Open with LinuxGitShell` |
| Several local items | all warm and in the same repository | `LinuxGitShell` ▸ `Show Status` for the repository root |
| Several local items | any cold, outside, or in another repository | none; cold items are refreshed |

When a merge, rebase, cherry-pick, revert, or bisect is in progress, the submenu lists it as a
disabled notice below `Show Status`. `Commit`, `Pull`, `Push`, `Show Log`, `Settings`, `Git Clone`,
and `Create repository here` will be added when the application implements them, so that the menu
never offers an action that cannot run.

Triggered actions start the external `linuxgitshell` application with one path as a separate
process argument. They never use a shell command string, and the application performs its own
repository discovery.

This increment proves loading, metadata, selection transfer, localization, and process isolation.
Repository-dependent entries come from the non-blocking context client described in
[`repository-context-cache.md`](repository-context-cache.md#implemented-client). Dynamic `Show Status`, `Commit`, `Pull`, `Push`,
`Show Log`, and `Settings` actions require an asynchronous external context resolver or the later
D-Bus service; they must not be implemented by running Git synchronously inside Dolphin.

Repository detection must also account for `.git` being either a directory or an indirection file,
and for bare repositories without a conventional worktree marker. The plugin will not inspect these
forms itself. The external-service, local-snapshot, cold-cache, invalidation, and latency design is
specified in [`repository-context-cache.md`](repository-context-cache.md).

CTest loads the built module through `KPluginFactory`, checks its metadata and conservative action
policy, and triggers the action against an isolated helper. The launch test proves that a path with
spaces, Unicode, and a newline reaches the external process unchanged as exactly one argument.

## Development installation

The install prefix must be selected during configuration. Changing only `cmake --install --prefix`
is too late because ECM calculates the Qt plugin directory while configuring.

```bash
cmake -S . -B build-dolphin -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX="$PWD/install"
cmake --build build-dolphin -j
ctest --test-dir build-dolphin --output-on-failure
cmake --install build-dolphin
```

For a prefix outside Qt's system prefix, ECM installs the module below `lib/plugins`. Expose both the
application and plugin to the development session:

```bash
export PATH="$PWD/install/bin:$PATH"
export QT_PLUGIN_PATH="$PWD/install/lib/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
kbuildsycoca6
```

The session bus only activates services listed in its own data directories, so the development
prefix's `share/dbus-1/services` file is not used. When testing the experimental context service,
start `./install/bin/linuxgitshell-daemon` manually and stop it when finished.

For a staged system-layout check, configure with `-DCMAKE_INSTALL_PREFIX=/usr` and use `DESTDIR`.
On the validated Arch/Manjaro environment, the resulting module path is
`usr/lib/qt6/plugins/kf6/kfileitemaction/linuxgitshell_fileitemaction.so` inside the staging root,
next to `usr/bin/linuxgitshell-daemon` and
`usr/share/dbus-1/services/org.linuxgitshell.Experimental.Context1.service`.

```bash
cmake -S . -B build-system -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-system -j2
ctest --test-dir build-system --output-on-failure
DESTDIR="$PWD/package-root" cmake --install build-system
```

Until an Arch/Manjaro package exists, this staged layout is the supported system-installation
verification. Inspect `build-system/install_manifest.txt` and the staging root; do not manually copy
development artifacts into `/usr`. A future package must own installation, upgrades, and removal.

## Verification status

As of 2026-09-24:

- the complete local and hosted CI suites pass 17/17 tests, including `clang-tidy`;
- CTest loads the real module through `KPluginFactory` and validates its metadata and actions;
- an unusual selected path reaches an external helper unchanged as one argument;
- on a private bus with the activatable context service, the real module shows the generic action
  while cold, then `Show Status` for repositories, operation notices, no entry outside
  repositories, the repository root for same-repository selections, and no entry across
  repositories;
- building 200 menus neither runs Git nor contacts the service before returning, with an offscreen
  `actions()` p95 of about 0.03 ms;
- clean development-prefix and staged `/usr` layouts contain the expected plugin, daemon, and
  activation file;
- interactive right-click behavior and native latency on Plasma Wayland remain pending.

## Manual test checklist

Use a disposable Git repository and close existing Dolphin windows before changing plugin search
paths. `kquitapp6 dolphin` requests a clean Dolphin shutdown; starting Dolphin again loads the new
module.

Start `linuxgitshell-daemon` from the development prefix first, because the session bus does not
read activation files from it.

- [ ] The first right-click on a repository root, a subdirectory, and a file shows
  `Open with LinuxGitShell`; a later right-click on each shows `LinuxGitShell` ▸ `Show Status`.
- [ ] Trigger each action and confirm the external application inspects the selected path.
- [ ] A directory outside any repository shows no LinuxGitShell entry after the first right-click.
- [ ] A bare repository directory shows `Show Status`, and the application reports a bare repository.
- [ ] During a merge or rebase, the submenu shows the operation as a disabled notice.
- [ ] Two files in one repository show `Show Status` for the repository root after the first
  right-click; files from two repositories show no entry.
- [ ] A remote KIO URL shows no LinuxGitShell action.
- [ ] Without a running daemon, menus keep showing `Open with LinuxGitShell` and Dolphin stays
  responsive.
- [ ] Closing LinuxGitShell leaves Dolphin running and responsive.
- [ ] Starting Dolphin without the development `PATH` produces a handled launch error and no crash.
- [ ] Spanish locale displays `Abrir con LinuxGitShell` and `Mostrar estado`.

To remove the development installation, shut down Dolphin and any manually started
`linuxgitshell-daemon`, remove only the repository-local `install/` directory, restore `PATH` and `QT_PLUGIN_PATH`, run `kbuildsycoca6`, and start Dolphin
again. Do not delete anything below `/usr` for this development workflow.
