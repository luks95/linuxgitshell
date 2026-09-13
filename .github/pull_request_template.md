## Summary

Describe the behavior change and link the relevant issue or design discussion.

## Verification

- [ ] Configure and build pass.
- [ ] CTest passes.
- [ ] Formatting and static checks pass.
- [ ] Tests cover the change proportionally to its risk.
- [ ] UI changes include screenshots and keyboard/theme checks.
- [ ] Dolphin changes keep Git, filesystem discovery, blocking IPC, caches, and windows outside the
      plugin process and report native manual-test results when applicable.
- [ ] Install-layout changes are checked with the prefix selected during configuration and a staged
      `DESTDIR` installation.
- [ ] Documentation, translations, `STATUS.md`, and `CHANGELOG.md` are updated when applicable.
- [ ] Logs and diagnostics contain no credentials or private repository data.
- [ ] New code and assets have compatible licenses and attribution.
- [ ] Every commit includes a DCO `Signed-off-by` line.
