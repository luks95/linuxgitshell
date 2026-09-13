# Repository path handling

LinuxGitShell delegates repository discovery to Git with `git -C` and `git rev-parse`. It does not
walk parent directories looking for `.git`, assume that `.git` is a directory, compare filesystem
device identifiers, or stop at mount boundaries. Consequently, a worktree and its Git directory may
reside in different locations or filesystems as long as Git can access both.

The discovery service preserves the absolute path supplied by the caller for diagnostics. It uses
the canonical target only as Git's working directory. This distinction is important for a symlink to
a file: the link can live outside the repository while its target lives inside it.

Paths are never lowercased or compared case-insensitively. On a case-sensitive filesystem,
`Repository` and `repository` remain distinct repositories. On a case-insensitive filesystem, Git
and the filesystem define the resulting identity.

No application-defined path-length limit is imposed. The effective limit is the lowest limit of the
operating system, filesystem, Qt, and Git. Automated coverage currently exercises discovery from a
path longer than 1,400 characters.

The integration tests also cover directory and file symlinks and a worktree whose `.git` file points
to a separate Git directory. They use temporary repositories and require no privileged mounts.
Performance and watcher behavior on external and network mounts remain separate later-phase work;
see the overlays and daemon phases in the roadmap.
