# albdf test harness (M2)

Deterministic regression harness for the fork. Everything here is committed;
fixtures and goldens are reproducible from `scripts/`.

## Layout

| Path | Contents |
|---|---|
| `fixtures/` | Byte-deterministic PDF corpus (no timestamps, fixed object order). |
| `golden/` | 72 dpi PNG renders, one per fixture page, named `<stem>_Image_<k>.png`. |
| `scripts/` | `pdfgen.py` (deterministic PDF writer) + one generator per fixture. |
| `smoke.sh` | CLI smoke test: info/fetch-text/render/unite invariants on the corpus (+ recognize-text/delete-object when present). |

## Fixture corpus

| Fixture | Pages | Purpose |
|---|---|---|
| `test-baseline.pdf` | 2 | Pre-fork baseline; goldens are the M0.5 upstream reference (see `docs/research/baseline-upstream.md`). |
| `multipage.pdf` | 5 | Multi-page doc, distinct text + vector shape per page. |
| `image-doc.pdf` | 2 | Embedded image XObject (40x30 RGB, FlateDecode) + captions. |
| `overlap-text.pdf` | 1 | Three overlapping text lines (same baseline, rotated). |

## Regenerating

```sh
python3 scripts/make-multipage-pdf.py  /tmp/multipage.pdf   # or any output path
python3 scripts/make-image-pdf.py      /tmp/image-doc.pdf
python3 scripts/make-overlap-pdf.py    /tmp/overlap-text.pdf
```

Generated files are byte-identical across runs (verify: run twice, `sha256sum`
both). `test-baseline.pdf` is not generated - it is the preserved M0.5 fixture
(`sha256 82298ab8c610c10bdc2527b5c09fa64340f4a4a97f2ec3ec89f2c8a3d4f0627f`).

## Running

```sh
# golden-image regression (QtTest, wired into ctest as UnitTestsGolden)
QT_QPA_PLATFORM=offscreen ./build/bin/UnitTestsGolden

# CLI smoke (wired into ctest as SmokeCli)
QT_QPA_PLATFORM=offscreen src/tests/smoke.sh

# everything via ctest
cd src/build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```

## Golden policy

A golden mismatch means renderer output changed and must be investigated.
**Never update goldens to silence a failure** - the `test-baseline.pdf` goldens
are the pre-fork upstream reference and must hash-match exactly; new-fixture
goldens are fork-anchored regression anchors. To (deliberately) re-anchor,
render at 72 dpi with the current `albdf`, replace the files, and document
the change in the commit message.
