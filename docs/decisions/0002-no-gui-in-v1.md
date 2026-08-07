# ADR-0002: No GUI in v1 — library + CLI only

**Status:** accepted (2026-08-04)

## Context

User directive: "forget about the gui, we only care about the library for now."
The project is a big agent-written codebase; the core must be testable, deterministic, and
headless. PDF4QT ships GUI apps we are stripping (see ADR-0001).

## Decision

v1 ships only the headless library + CLI. No QWidgets, no QML, no web frontend.
Every capability must be reachable from a shell command and testable with
`QT_QPA_PLATFORM=offscreen`. A future GUI (thin Qt shell or PDF4QT's existing apps)
consumes the library.

## Consequences

- 90% of development/test happens without a display — ideal for agents.
- Deterministic golden tests possible.
- CLI is independently shippable (qpdf/mutool precedent).

## Addendum (M12, 2026-08-07): optional GUI target — decision stands

The **decision stands**: v1 ships the headless library + CLI, and the default build
contains no GUI. M12 added the vendored PDF4QT GUI layers (`Pdf4QtLibWidgets`,
`Pdf4QtLibGui`, `Pdf4QtEditor`, `Pdf4QtViewer`, `Pdf4QtPageMaster`) as an **optional**
build target gated behind `-DALBDF_BUILD_GUI=ON` (default OFF). The headless default
is preserved: core + CLI + tests never link or include GUI code and must remain
deterministic and display-free regardless of the flag. The GUI is a thin consumer of
the same library the CLI uses, per the original "future GUI consumes the library"
rationale — not a return to GUI-first development.
