# Third-party notices — albdf

albdf's own code is **GPL-3.0-or-later**; the vendored PDF4QT-derived portions
remain **MIT**. It links the components below.

Corresponding source for every component is available from its upstream project
(linked below) and is built unmodified by vcpkg from the pinned baseline in
`src/vcpkg.json`; albdf's own source is at
<https://github.com/yolka-wiz/al-bdf-engine>.

## Bundled with the macOS release

| Component | License | Source |
| --- | --- | --- |
| Qt 6 (Core, Gui, Xml, Svg) | LGPL-3.0-only (or GPL-2.0/3.0 / commercial) | <https://code.qt.io/cgit/qt/> |
| fontconfig | MIT-style (HPND) | <https://gitlab.freedesktop.org/fontconfig/fontconfig> |

Qt is bundled on macOS only, because macOS has no system Qt. The bundled
libraries are unmodified, are loaded dynamically at run time by install name
(`@rpath/...`), and can be replaced with a modified Qt by replacing the
files under `Frameworks/` — which is what the LGPL-3.0 requires to be possible.
Qt's own license texts ship inside each framework
(`Frameworks/*.framework/Versions/A/Resources/`).

On Linux **no Qt is bundled**: the distribution's Qt is used
(`docs/PREREQUISITES.md`), so Qt's own license terms are whatever your
distribution ships.

## Linked into `libPdf4QtLibCore` (static, all platforms)

| Component | License | Source |
| --- | --- | --- |
| FriBidi | **LGPL-2.1-or-later** | <https://github.com/fribidi/fribidi> |
| HarfBuzz | MIT | <https://github.com/harfbuzz/harfbuzz> |
| FreeType | FTL or GPL-2.0-or-later | <https://gitlab.freedesktop.org/freetype/freetype> |
| Blend2D | Zlib | <https://github.com/blend2d/blend2d> |
| OpenJPEG | BSD-2-Clause | <https://github.com/uclouvain/openjpeg> |
| OpenSSL | Apache-2.0 | <https://github.com/openssl/openssl> |
| oneTBB | Apache-2.0 | <https://github.com/uxlfoundation/oneTBB> |
| Little CMS 2 | MIT | <https://github.com/mm2/Little-CMS> |
| zlib | Zlib | <https://zlib.net/> |
| libjpeg-turbo | IJG / BSD-3-Clause / Zlib | <https://github.com/libjpeg-turbo/libjpeg-turbo> |
| libpng | libpng-2.0 | <http://www.libpng.org/pub/png/libpng.html> |

Also linked on Linux: ICU (Unicode License v3), via the system or the Qt build.

### The one item that needs care

**FriBidi is LGPL-2.1-or-later and is linked statically** into
`libPdf4QtLibCore`, which is why this section exists rather than a bare list of
names. LGPL-2.1 §6 expects a recipient to be able to relink the work against a
modified FriBidi. Two things cover it here:

1. LGPL-2.1 §3 permits applying the GNU GPL instead of the LGPL to a combined
   work. albdf as a whole is distributed under **GPL-3.0-or-later**, so the
   corresponding-source route (the public repository, at the released tag)
   is what governs.
2. Nothing here is modified: the FriBidi revision is the upstream one pinned by
   the vcpkg baseline, so rebuilding it — or replacing it with a modified copy
   and relinking — needs no changes to albdf.

If you distribute albdf commercially or must satisfy a stricter reading of
LGPL §6, the alternative is to make FriBidi a dynamically linked dependency
(`libfribidi.so.0` / `libfribidi.0.dylib`), which removes the question entirely.
That is a build change, not a packaging one, and is deliberately not done here.
