# ADR-0004: License posture — MIT fork + permissive deps only

**Status:** accepted (2026-08-04)

## Context

The fork inherits MIT from PDF4QT. The project must stay clean for permissive distribution.
No AGPL/GPL contamination (research report §2.4: MuPDF AGPL, Poppler GPL — both rejected
for linking into a permissive core).

## Decision

- Core remains MIT. Every new file carries the MIT header.
- Permitted deps: Qt (LGPL-3, dynamic), FreeType (FTL), OpenJPEG (MIT), OpenSSL (Apache-2.0),
  ZLIB, TBB, blend2d (inherited from PDF4QT); HarfBuzz (MIT) and FriBidi (LGPL-2.1, dynamic
  linking) to be added.
- NO AGPL/GPL libraries linked into the core. No new dependency without an ADR + approval.
- The future GUI phase may revisit (e.g., GPL Qt apps are common on KDE) but the library stays MIT.

## Consequences

- Any downstream can take the library (commercial or OSS).
- Some advanced features (MuPDF-grade redaction/signing) stay out unless implemented ourselves
  or shelled out as subprocess — redaction/signing already exist in PDF4QT core, so low impact.
