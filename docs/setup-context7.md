# context7 MCP wiring (Hermes side)

> How to configure the context7 MCP server for the Hermes agent runtime.
> The API key is a **secret** — it must never be committed to this repo.

## Where the config lives

The `mcp_servers` config lives in the Hermes profile config
(`~/.hermes/profiles/yolka/config.yaml`). The context7 API key is a
**secret** — it must NOT be committed to the repo (a live key was removed in
2026-08-05; rotate it if you ever paste it into a tracked file). Store it via
the Hermes CLI so it lands only in the local config:

```bash
hermes config set mcp_servers.context7.url "https://mcp.context7.com/mcp"
hermes config set mcp_servers.context7.headers.Authorization "Bearer <CONTEXT7_API_KEY>"
```

## Verify the connection

No restart needed for the test:

```bash
hermes mcp list        # context7 should show enabled
hermes mcp test context7
```

## Prereqs

Hermes runtime must have MCP client support:

```bash
pip install mcp        # MCP client support; without it MCP is silently disabled
```

## Load the tools into sessions

Then **restart Hermes** to load the tools into sessions (MCP servers load at
startup; no hot-reload). Tools appear as `mcp_context7_*`.

Docs: https://context7.com/docs/mcp
