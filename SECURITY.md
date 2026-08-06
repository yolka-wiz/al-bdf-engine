# Security Policy

## Reporting a vulnerability

Please **do not open a public issue** for security vulnerabilities.

Report privately to the maintainers via GitHub's **Security Advisory** flow:

1. Open the repository on GitHub.
2. Go to **Security → Report a vulnerability**.
3. Provide:
   - Affected command/component and version (or commit SHA),
   - A minimal reproducer (command + input file, if possible),
   - Impact and suggested fix, if you have one.

Alternatively, email the maintainer directly — the address is listed on the
GitHub profile of the repository owner.

You should receive an acknowledgement within **48 hours**. We will coordinate
a fix and a release, then disclose the issue publicly (advisory + CVE where
appropriate) after the fix is available.

## Scope

- The `albdf` CLI binary (`src/PdfTool/`)
- The PDF library core (`src/Pdf4QtLibCore/`) — parsing, rendering, content
  editing, RTL text pipeline, forms, signatures
- Build/CI scripts in this repository

Out of scope: third-party dependencies (report those to their own projects),
and GUI applications (deferred per ADR-0002).

## Security notes for contributors

- **Never commit secrets** — API keys, tokens, passwords, certificates, or
  private keys. A public repo is scraped within minutes. If you ever push a
  secret, tell a maintainer immediately; we will rotate it and scrub history.
- Malformed-PDF handling is a security boundary: the library must never crash,
  hang, or read out of bounds on hostile input. Fuzz harnesses and ASAN/UBSAN
  runs are part of CI (`ci/run-ci.sh`).
- Signature verification (`verify-signatures`) must fail closed on tampered
  documents.

## Supported versions

| Version | Supported |
|---|---|
| main (unreleased) | ✅ |
| 0.1.x | ✅ security fixes |
| < 0.1.0 | ❌ |
