#!/usr/bin/env python3
"""Generate image-doc.pdf: 2-page deterministic fixture with an embedded
image XObject (40x30 RGB, FlateDecode, no PNG wrapper) on page 1 and a
caption text line on both pages.

Image pattern is a pure function of (x, y): horizontal gradient +
diagonal stripes + a solid red block — deterministic across runs.

Regenerable: python3 make-image-pdf.py [output.pdf]
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pdfgen

W, H = 40, 30


def make_pixels():
    rows = []
    for y in range(H):
        row = bytearray()
        for x in range(W):
            if x < 10 and y < 8:
                r, g, b = 220, 20, 20          # solid red block
            elif (x + y) % 9 < 2:
                r, g, b = 0, 0, 0              # diagonal black stripes
            else:
                r = int(255 * x / (W - 1))     # horizontal gradient
                g = int(255 * y / (H - 1))     # vertical gradient
                b = 128
            row += bytes((r, g, b))
        rows.append(bytes(row))
    return b"".join(rows)


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "image-doc.pdf"
    gen = pdfgen.PdfGen()

    # Object layout: 1 = Catalog, 2 = Pages tree (placeholder), then objects.
    gen.add_object(b"<< /Type /Catalog /Pages 2 0 R >>")
    gen.add_object(b"")

    # Shared font.
    font_ref = gen.add_object(pdfgen.FONT_HELVETICA)

    # Embedded image XObject: raw RGB samples, FlateDecode (deterministic zlib).
    image_ref = gen.add_object(gen.stream(make_pixels(), flate=True,
                                          extra=pdfgen.image_xobject_dict(W, H)))

    # Page 1: image drawn at 150x112.5 pt + caption.
    content1 = (b"q 150 0 0 112.5 231 360 cm /Im1 Do Q\n"
                b"BT /F1 16 Tf 72 700 Td (EMBEDDED IMAGE DOCUMENT) Tj ET\n")
    content1_ref = gen.add_object(gen.stream(content1))
    res1_ref = gen.add_object(pdfgen.resources_body([(b"F1", font_ref)], [(b"Im1", image_ref)]))
    page1_ref = gen.add_object(pdfgen.page_body(2, res1_ref, content1_ref))

    # Page 2: plain text page.
    content2 = b"BT /F1 18 Tf 72 720 Td (IMAGE DOCUMENT SECOND PAGE) Tj ET\n"
    content2_ref = gen.add_object(gen.stream(content2))
    res2_ref = gen.add_object(pdfgen.resources_body([(b"F1", font_ref)]))
    page2_ref = gen.add_object(pdfgen.page_body(2, res2_ref, content2_ref))

    kids = b"%d 0 R %d 0 R " % (page1_ref, page2_ref)
    gen.overwrite_object(2, b"<< /Type /Pages /Kids [%s] /Count 2 >>" % kids)

    gen.write(out_path)


if __name__ == "__main__":
    main()
