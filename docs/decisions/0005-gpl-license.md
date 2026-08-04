# ADR-0005: Relicense to GPL-3.0-or-later

**Status:** accepted (2026-08-04)
**Supersedes:** ADR-0004 (MIT fork + permissive deps only)

## Context

The project's license posture was MIT (inherited from the PDF4QT fork, ADR-0004),
so any downstream could take the code commercially or into closed-source products.
The project owner decided they prefer **GPL** — the code stays free and open
forever, and any derivative that ships to users must also be GPL.

Relicensing our own code from MIT to GPL is permitted (MIT is permissive; the
copyright holder may relicense their own contributions under any terms).

## Compatibility analysis (why GPL-3, not GPL-2)

The binding constraint is **OpenSSL (Apache-2.0)**, linked by the core for
encryption/decryption and signatures. The FSF considers Apache-2.0:

- **incompatible with GPL-2.0** (Apache's patent grant clause conflicts with
  GPLv2's terms), but
- **compatible with GPL-3.0** (GPLv3's patent provisions are compatible with
  Apache-2.0).

Therefore the project MUST use **GPL-3.0 (or any later version)**. A GPL-2.0
license would make linking OpenSSL a license violation. The `-or-later` suffix
leaves room for future GPLv4.

All other linked dependencies are GPLv3-compatible:
Qt (LGPL-3, dynamic), FreeType (FTL), OpenJPEG (MIT), LCMS2 (MIT), zlib,
libjpeg-turbo (IJG/BSD), libpng (libpng-2.0), TBB (Apache-2.0), blend2d (Zlib),
HarfBuzz (MIT), FriBidi (LGPL-2.1, dynamically linked).

## Decision

- **The project is licensed GPL-3.0-or-later.** A `LICENSE` file (GNU GPLv3)
  is at the repo root.
- **Our authored files** carry a GPL-3.0-or-later header (`SPDX-License-Identifier:
  GPL-3.0-or-later`).
- **Vendored upstream PDF4QT files** keep their MIT headers — we do not hold
  copyright on Jakub Melka's code. MIT is GPLv3-compatible, so the combined
  work is GPLv3 while upstream portions stay MIT (with attribution preserved).
- **Dependency rule unchanged:** no GPLv2-incompatible or copyleft-viral
  dependency without a fresh ADR. All current deps are GPLv3-compatible.

## Consequences

- Any downstream distributing the binary or a derivative **must** release under
  GPLv3 (or later), provide source, and keep the license notices.
- Downstream may no longer take the core into a closed-source/proprietary
  product without also GPL-licensing their derivative.
- Upstream PDF4QT (MIT) can still be forked separately by others under MIT;
  our fork's *additions* are GPL. Cherry-picks from upstream remain MIT and are
  compatible.
- A future GUI phase (GPL, as is common on KDE) is now trivially compatible.
