# ADR-0002: No GUI in v1 — library + CLI only

**Status:** proposed (2026-08-04)

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
