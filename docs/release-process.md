# Release process

This process is intentionally maintainer-driven. The artifact workflow validates and packages a tag,
but it does not create tags or publish a GitHub release automatically.

## Prepare the release commit

1. Confirm the version in the root `CMakeLists.txt` and the expected version in
   `tests/ProjectInfoTest.cpp` match the intended `vX.Y.Z` tag.
2. Add `docs/releases/vX.Y.Z.md` with highlights, limitations, requirements, and verification notes.
3. Move the release changes from `Unreleased` to a dated heading in `CHANGELOG.md` only when the
   release commit is ready to tag.
4. Update `STATUS.md`, but do not claim that the release exists before its public artifacts do.
5. Confirm private security reporting, the support channel, repository topics, required CI, and
   branch protection are configured on the forge. Require sign-off for commits created in the web
   interface when the forge supports it.

## Verify the exact candidate

Start from a clean checkout of the release commit and use a fresh out-of-source directory:

```bash
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j2
ctest --test-dir build-release --output-on-failure
DESTDIR="$PWD/build-release/stage" cmake --install build-release --prefix /usr
cmake --build build-release --target format-check
cmake --build build-release --target check-secrets
QT_QPA_PLATFORM=offscreen ./build-release/linuxgitshell --version
```

Confirm that the staged installation contains `usr/bin/linuxgitshell` and the Spanish catalog under
`usr/share/locale/es/LC_MESSAGES`. The printed version must be `linuxgitshell X.Y.Z`. Also complete
`docs/manual-smoke-test.md` in KDE Plasma 6 Wayland and record the tested commit and environment.
Do not release if a required check fails or the worktree is dirty.

## Tag and build artifacts

Create a signed, annotated tag when a configured signing identity is available:

```bash
git tag --sign vX.Y.Z -m "LinuxGitShell X.Y.Z"
git push origin vX.Y.Z
```

If signed tags are temporarily unavailable, document that fact in the release and create an annotated
tag with `git tag --annotate`; never use a lightweight release tag. Pushing the tag starts the
`Release artifacts` workflow. It rejects a tag that does not match the compiled application version
or lacks its release-notes file.

Download the workflow artifact and verify it locally:

```bash
sha256sum --check SHA256SUMS
tar --extract --gzip --file LinuxGitShell-X.Y.Z.tar.gz
cmake -S LinuxGitShell-X.Y.Z -B archive-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build archive-build -j2
ctest --test-dir archive-build --output-on-failure
```

## Publish and record

Create the forge release from the existing tag, use `docs/releases/vX.Y.Z.md` as its notes, and attach
the source archive and `SHA256SUMS` without renaming them. Download the public copies once more and
verify the checksum. Then update `STATUS.md`, `CHANGELOG.md`, and `roadmap-checklist.md` with the public
release URL and the evidence for completed checks.

If publication must be withdrawn, preserve the immutable tag and publish a clear notice; fix code in
a new patch release instead of replacing an existing artifact.
