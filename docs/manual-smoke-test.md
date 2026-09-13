# Manual application smoke test

Use this checklist on a supported KDE Plasma 6 Wayland session after the automated tests pass.
The test repositories must be disposable; never use a contributor's personal repository.

## Preparation

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Create a temporary repository with Git's normal command-line tools, including at least one commit,
one staged file, one modified tracked file, and one untracked file. Record its path for the checks
below.

## Checklist

- Run `./build/linuxgitshell /path/to/temporary/repository` and confirm the window opens without
  blocking Plasma or Dolphin.
- Confirm the repository root, branch, repository type, and counts for staged, modified, untracked,
  and conflicted files match `git status`.
- Confirm the configuration table contains the expected local entries and their origins. Values for
  credential-bearing URLs, secret-like keys, and `http.extraheader` must display as `REDACTED`.
- Select **Reload** after changing the temporary repository and confirm the status summary updates.
- Open a clean temporary repository and confirm the application reports **Working tree clean**.
- Open an existing directory that is not a Git repository and confirm a localized error plus Git's
  diagnostic output are shown.
- Start the application without a path and confirm it explains how to provide one without starting
  repository inspection.
- Repeat once with `LANGUAGE=es` and confirm the visible application text is translated into Spanish.

Close the application normally. Remove only the disposable repositories created for this checklist.

## Latest verification

The `v0.1.0` candidate at commit `4419b3c` was verified on 2026-09-13 in a native KDE Plasma 6
Wayland session on Manjaro with Qt 6.11.2 and KDE Frameworks 6.29.0.

- The application opened and remained responsive on the Wayland backend.
- A sanitized disposable repository displayed branch `main`, normal repository type, and the
  expected staged/modified/untracked/conflict counts of `1/1/1/0`.
- Local configuration loaded successfully and `http.extraheader` displayed as `REDACTED`.
- Reload, clean repository, invalid repository, and missing-path behavior passed the native Wayland
  application test.
- The installed Spanish catalog rendered the no-path view in Spanish.

Temporary repositories, logs, staged installation files, and screenshots were removed after the
verification.
