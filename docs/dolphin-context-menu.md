# Dolphin Context-Menu Integration

Implementation status: the minimal single-local-selection plugin is merged on `main` through
[PR #9](https://github.com/luks95/linuxgitshell/pull/9). Repository-aware menus remain planned work
under [issue #12](https://github.com/luks95/linuxgitshell/issues/12).

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

The first plugin increment is deliberately small:

1. Return no actions for an empty selection, a remote URL, or more than one selected item.
2. For one local item, return one translated `Open with LinuxGitShell` action immediately.
3. When triggered, start the external `linuxgitshell` application with the local path as a separate
   process argument. Never use a shell command string.
4. Let the external application perform repository discovery and display its typed error if the path
   is not inside a repository.
5. Keep selection policy in a UI-independent helper so it can be unit tested without Dolphin.

This increment proves loading, metadata, selection transfer, localization, and process isolation.
It does not yet claim repository-dependent menus. Dynamic `Show Status`, `Commit`, `Pull`, `Push`,
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

For a staged system-layout check, configure with `-DCMAKE_INSTALL_PREFIX=/usr` and use `DESTDIR`.
On the validated Arch/Manjaro environment, the resulting module path is
`usr/lib/qt6/plugins/kf6/kfileitemaction/linuxgitshell_fileitemaction.so` inside the staging root.

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

As of 2026-09-13:

- the complete local and hosted CI suites pass 11/11 tests;
- CTest loads the real module through `KPluginFactory` and validates its metadata and actions;
- an unusual selected path reaches an external helper unchanged as one argument;
- clean development-prefix and staged `/usr` layouts contain the expected plugin;
- Dolphin starts offscreen with the development plugin path in isolated D-Bus/XDG state;
- interactive right-click behavior on native Wayland remains pending.

## Manual test checklist

Use a disposable Git repository and close existing Dolphin windows before changing plugin search
paths. `kquitapp6 dolphin` requests a clean Dolphin shutdown; starting Dolphin again loads the new
module.

- [ ] Right-click one repository root, one subdirectory, and one file; each shows
  `Open with LinuxGitShell`.
- [ ] Trigger each action and confirm the external application inspects the selected path.
- [ ] A two-item selection shows no LinuxGitShell action.
- [ ] A remote KIO URL shows no LinuxGitShell action.
- [ ] Closing LinuxGitShell leaves Dolphin running and responsive.
- [ ] Starting Dolphin without the development `PATH` produces a handled launch error and no crash.
- [ ] Spanish locale displays `Abrir con LinuxGitShell`.

To remove the development installation, shut down Dolphin, remove only the repository-local
`install/` directory, restore `PATH` and `QT_PLUGIN_PATH`, run `kbuildsycoca6`, and start Dolphin
again. Do not delete anything below `/usr` for this development workflow.
