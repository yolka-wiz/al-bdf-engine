# albdf release prerequisites

What a **prebuilt** albdf tarball needs on the machine that runs it. Building
from source needs more — see `README.md`.

## macOS

Nothing. The tarball carries the Qt frameworks, fontconfig and the platform
plugins it uses, and every reference inside it is relative to the bundle, so it
runs on a Mac that has never had Homebrew Qt installed.

These binaries are unsigned, so Gatekeeper quarantines a downloaded tarball. If
macOS refuses to run `albdf`, clear the attribute:

```bash
xattr -d com.apple.quarantine bin/albdf
```

## Linux — Debian / Ubuntu

```bash
sudo apt install libqt6core6 libqt6gui6 libqt6xml6 libfontconfig1 \
                 libglvnd0 libopengl0 qt6-qpa-plugins
```

## Linux — Fedora

```bash
sudo dnf install qt6-qtbase qt6-qtbase-gui fontconfig libglvnd-glx libglvnd-opengl
```

## Why these, and why they are not bundled

The base C runtime (glibc, libstdc++, libgcc, libm) exists on every installation
and is never shipped. Everything else is chosen so that albdf does not put a
second copy of a distribution-maintained library on your machine:

- **Qt 6** (`libQt6Core` / `libQt6Gui` / `libQt6Xml`) — one `apt`/`dnf` line away
  on every desktop distribution, and already the copy your desktop loads. Using
  the system's Qt means there is exactly one Qt in the process; shipping another
  is how conflicts start. (macOS is the opposite case and is bundled: it has no
  system Qt to use.)
- **fontconfig** — present on every desktop distribution; albdf uses it for
  system-font substitution.
- **`libglvnd0` + `libopengl0`** (Fedora: `libglvnd-glx`, `libglvnd-opengl`;
  they provide `libGLX.so.0` and `libOpenGL.so.0`) — required by **Qt6Gui**,
  not used by albdf: there is no OpenGL code in the CLI or the core library.
  These are transitive, and the loader needs them present for Qt6Gui to load.
- **`qt6-qpa-plugins`** — supplies Qt's platform plugins. albdf never opens a
  window: with no `$DISPLAY` or `$WAYLAND_DISPLAY` it selects the offscreen
  platform itself, so it runs in a container or a CI pipeline with no
  environment variable set. An explicit `QT_QPA_PLATFORM` always takes
  precedence.

`qt6-svg` is **not** required on Linux.

Everything albdf links statically (FriBidi, HarfBuzz, FreeType, OpenJPEG,
OpenSSL, TBB, LCMS2, zlib, libjpeg-turbo, libpng, blend2d) is already inside
`libPdf4QtLibCore` and needs nothing installed.
