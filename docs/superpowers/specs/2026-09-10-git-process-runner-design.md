# Git Process Runner Design

## Scope

This iteration implements only Phase 1.1 of the roadmap: a reusable, asynchronous process runner for Git commands and its automated tests. Repository discovery, status parsing, configuration reading, and GUI integration remain outside this change.

The runner must keep Git execution out of widgets and future Dolphin plugins. It must pass the executable and every argument separately to `QProcess`; it must never invoke a shell or construct a shell command.

## Chosen approach

Use one `GitProcessRunner` instance per active execution. The runner is a `QObject` that owns one `QProcess` and the timers needed for timeout and cancellation escalation.

This design matches Qt's event-driven API, gives every operation an explicit lifetime, and makes accidental state sharing between concurrent commands impossible. Callers that need concurrency create multiple runners. A higher-level operation service can own those runners later without changing the request and result models.

Alternatives considered:

- A shared service returning operation handles would make bulk concurrency convenient, but it adds lifecycle and collection management before any consumer needs them.
- A `QFuture` API would compose final results well, but makes incremental output and process-specific cancellation less direct.

## Public model

`GitProcessRequest` contains:

- the program path or name, defaulting to `git`;
- an ordered `QStringList` of arguments;
- an optional working directory;
- a `QProcessEnvironment`, initialized from the system environment unless explicitly replaced;
- an optional positive timeout.

`GitProcessResult` contains:

- the original stdout and stderr bytes without sanitization or transcoding;
- the process exit code and `QProcess::ExitStatus` when available;
- a typed completion reason: `Completed`, `FailedToStart`, `Crashed`, `Cancelled`, or `TimedOut`;
- the original `QProcess` error and error string when available.

The completion reason describes how the operation ended. A normally exited Git process with a nonzero exit code is still `Completed`; callers interpret Git's exit code in the context of the command.

`GitProcessStartResult` reports whether a request was `Accepted`, rejected because the runner is `Busy`, or rejected because its program is empty. This keeps pre-start validation distinct from the terminal result of an accepted execution.

## Runner lifecycle

`start(request)` is asynchronous and returns a `GitProcessStartResult` immediately. It validates that the runner is idle and that the program is nonempty, configures the child process, starts it, and emits `started` only after `QProcess` confirms startup.

The runner exposes signals for incremental stdout, incremental stderr, and one terminal `finished(result)` notification. It also accumulates both streams so the terminal result always contains the complete original output.

A runner accepts only one active request. Starting it while busy returns `Busy` and does not emit a terminal signal or disturb the existing process. Each accepted request emits exactly one terminal `finished(result)` signal. After terminal completion the runner can be reused.

## Cancellation and timeout

`cancel()` marks the active operation as cancelled and requests graceful termination with `terminate()`. If the child remains alive after a short fixed grace period, the runner calls `kill()`. Calling `cancel()` while idle has no effect.

A timeout is opt-in per request. When it expires, the runner follows the same terminate-then-kill sequence but records `TimedOut`, not `Cancelled`. The first terminal cause wins, preventing a later process signal from changing the reported reason.

All timers are stopped during terminal cleanup. Destruction of a busy runner terminates the owned process without starting a nested event loop.

## Logging and diagnostics

The raw request and raw process output remain available to the caller and are not written to logs automatically.

A separate sanitization function produces a display-safe argument list for opt-in diagnostic logging. For HTTP(S) URL arguments it removes user information and redacts query values whose key contains `token`, `password`, or `secret`. It also redacts values in `--password`, `--token`, `--oauth-token`, and `--private-token`, accepting both `--option=value` and split `--option value` forms. It never includes the request environment. Returning a sanitized `QStringList` preserves argument boundaries and ensures the display representation is never fed back into process execution.

No stdout or stderr content is logged automatically because arbitrary repository output can contain secrets. Future diagnostic UI may show the original output only through an explicit user action.

## Errors

Startup validation and `QProcess::ProcessError` values are retained in the result. The core exposes stable typed causes rather than translated strings. User-facing translation belongs in the application layer, where common causes can be mapped to actionable messages while retaining the original technical detail.

The runner does not retry commands. Retries are command-specific policy and belong in higher-level services.

## Tests

Tests use a small C++ helper executable built with the test suite rather than a shell. The helper can echo arguments, environment, and working directory; write independently to stdout and stderr; exit with a requested code; or wait until terminated.

Automated tests cover:

- successful asynchronous execution and exactly one terminal signal;
- separate, complete stdout and stderr capture;
- nonzero exit codes without misclassifying them as process failures;
- startup failure;
- explicit environment and working directory handling;
- arguments and paths containing spaces, Unicode, and names beginning with a hyphen;
- explicit cancellation and forced termination fallback;
- opt-in timeout, distinguished from cancellation;
- runner reuse after completion and rejection while busy;
- credential redaction without alteration of the original request or output.

CTest registers the runner suite. Tests operate only inside temporary directories and never inspect or alter a contributor's repositories.

## Acceptance criteria

- No API blocks the calling thread while a Git process runs.
- Executables and arguments are passed directly to `QProcess` without a shell.
- Every accepted execution produces exactly one typed terminal result.
- Cancellation and timeout are safe and distinguishable.
- Original stdout and stderr remain byte-for-byte available.
- Diagnostic command formatting redacts credential-bearing values.
- The focused test suite and all existing tests pass.
- `roadmap-checklist.md`, `STATUS.md`, and `CHANGELOG.md` are updated when implementation is complete.
