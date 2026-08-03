# context7 MCP wiring (for the host macOS Hermes side)

The `mcp_servers` config lives in the Hermes profile config on the **host** (macOS),
which the yolka container cannot write. Run these on the Mac (or paste into
`~/.hermes/profiles/yolka/config.yaml` under `mcp_servers:`):

```yaml
mcp_servers:
  context7:
    url: "https://mcp.context7.com/mcp"
    headers:
      Authorization: "Bearer ***REDACTED: CONTEXT7_API_KEY***"
```

or via CLI:

```bash
hermes config set mcp_servers.context7.url "https://mcp.context7.com/mcp"
hermes config set mcp_servers.context7.headers.Authorization "Bearer ***REDACTED: CONTEXT7_API_KEY***"
```

Prereqs (host Hermes runtime):
```bash
pip install mcp        # MCP client support; without it MCP is silently disabled
```

Then **restart Hermes** (MCP servers load at startup; no hot-reload).
Tools appear as `mcp_context7_*`. Docs: https://context7.com/docs/mcp
