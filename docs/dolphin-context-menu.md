# Dolphin Context-Menu Integration

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
| `extra-cmake-modules` | 6.29.0-1 | available, not installed | Standard KDE install paths and CMake settings |

`extra-cmake-modules` is the only missing package for the standard plugin target. Its
`KDEInstallDirs6` and `KDECMakeSettings` modules provide `KDE_INSTALL_PLUGINDIR` and the library
output directory expected by `kcoreaddons_add_plugin`. Install it only when implementation begins.

## Process boundary

The first plugin increment must remain deliberately small:

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

## Verification target

The first implementation PR must verify the plugin output and install location, load it in Dolphin,
exercise root/subfolder/file selections, and confirm that closing or failing the external
LinuxGitShell process does not affect Dolphin. Development-prefix discovery and clean uninstallation
will be documented with the implementation because the exact installed paths are part of that test.
