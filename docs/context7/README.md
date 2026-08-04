# Context7 MCP usage notes (pdfedit)

Fetched 2026-08-04 via native `mcp__context7__*` tools (wired by user on host).

## How to query fresh docs

1. `resolve-library-id` with BOTH args:
   - `libraryName`: official name ("HarfBuzz", not "harfbuzz")
   - `query`: what you're trying to do (used for ranking)
2. `query-docs` with:
   - `libraryId`: e.g. `/harfbuzz/harfbuzz` (from step 1, or user-supplied `/org/project[/version]`)
   - `query`: one concept per call, specific
3. Max 3 resolve + 3 query calls per question.

## Library IDs discovered

| Library | ID | Reputation | Snippets |
|---|---|---|---|
| HarfBuzz | `/harfbuzz/harfbuzz` | High | 2024 |
| GNU FriBidi | `/fribidi/fribidi` | High | 119 |
| FreeType | `/freetype/freetype` | High | 149 |
| Rustybuzz (Rust HB port) | `/harfbuzz/rustybuzz` | High | 121 |
| HarfRust | `/harfbuzz/harfrust` | High | 1545 |

## Not available

- PDF4QT → "No libraries found" (project too small for Context7's corpus) —
  use upstream source in `vendor-upstream-pdf4qt/` as the authority instead.

## Raw-curl probes (container-side, pre-wiring)

`/workspace/scripts-tmp/mcp-test.sh` / `mcp-call.sh` / `mcp-docs.sh`:
- POST to `https://mcp.context7.com/mcp` with `Authorization: Bearer <key>`
- **SSE transport requires `Mcp-Session-Id` header** obtained from the
  `initialize` response (header name is lowercase `mcp-session-id` on this server;
  can be folded across lines — parse carefully)
- `tools/call` arg validation is strict (wrong arg names → -32602 with the
  expected path in the error, e.g. `libraryName` vs `query` — resolve-library-id
  needs BOTH)

These are now superseded by the native `mcp__context7__*` tools in-session.
