# D-Bus API compatibility policy

The daemon API is not implemented yet. When introduced, its well-known bus name and top-level
interfaces will carry a major version, initially `org.linuxgitshell.Daemon1`.

- Compatible methods, signals, properties, and optional fields may be added within a major version.
- Existing meanings, required fields, types, and error semantics must not change incompatibly.
- Clients must ignore unknown optional data and handle unavailable methods, timeout, restart, and
  daemon absence.
- An incompatible change requires a new interface or bus-name major version and a documented migration.
- The previous stable major version should remain available for at least one normal release cycle when
  practical; exceptions require a security or correctness justification in the changelog.
- Experimental interfaces must be labeled and must not use the stable name.

Introspection XML, threat boundaries, path validation, timeouts, and integration tests are required
before the first API is declared stable.
