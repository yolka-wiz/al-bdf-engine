# Agent Roles

This project is written by an agent team. Each role is a **contract**: when the orchestrator
dispatches a subagent for a task, the subagent receives the role definition + task + plan ref.
Roles are defined per concern so skills can be pinned per role (see `skills/` and the DB `skills` table).

| Role id | Concern | Typical tasks |
|---|---|---|
| `core-agent` | PDF core library (fork), text recognition, deletion, add-text | M1–M3 |
| `cli-agent` | CLI surface, commands, output format, determinism | M2–M3 |
| `rtl-agent` | RTL write pipeline + RTL search (HarfBuzz/FriBidi/ToUnicode) | M4–M5 |
| `test-agent` | Unit + golden-image tests, corpus, CI | M6 (parallel) |
| `research-agent` | Rosetta: deep research, API discovery, skill sourcing | continuous |
| `orchestrator` | Yolka: planning, dispatch, review, DB, ADRs | continuous |

Each role file: mission, scope (what you may touch), rules, skills to load, exit criteria.
