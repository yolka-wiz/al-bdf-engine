#!/usr/bin/env python3
"""Generate multipage.pdf: 5-page deterministic fixture, one distinct text
line + one distinct vector shape per page (Helvetica, no embedding).

Regenerable: python3 make-multipage-pdf.py [output.pdf]
Output is byte-deterministic (no timestamps, fixed zlib level).
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pdfgen

PAGES = [
    # (page text, content stream ops)
    ("MULTIPAGE PAGE ONE",   b"0.5 0.5 0.5 rg 72 650 200 50 re f\n"),
    ("MULTIPAGE PAGE TWO",   b"0.2 0.6 0.9 rg 200 500 60 60 re f\n"),
    ("MULTIPAGE PAGE THREE", b"0.9 0.4 0.1 rg 100 400 300 4 re f\n"),
    ("MULTIPAGE PAGE FOUR",  b"0.3 0.8 0.3 rg 150 300 80 40 re f\n"),
    ("MULTIPAGE PAGE FIVE",  b"0.6 0.2 0.7 rg 72 200 250 90 re f\n"),
]


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "multipage.pdf"
    gen = pdfgen.PdfGen()

    # Object layout: 1 = Catalog, 2 = Pages tree (placeholder, filled below),
    # then per-page objects. Page bodies reference parent 2 0 R.
    gen.add_object(b"<< /Type /Catalog /Pages 2 0 R >>")
    gen.add_object(b"")  # reserved slot for the /Pages tree (object 2)

    pages_refs = []
    for text, shape_ops in PAGES:
        content = (b"BT /F1 24 Tf 72 720 Td (%s) Tj ET\n" % text.encode()) + shape_ops
        content_ref = gen.add_object(gen.stream(content))
        font_ref = gen.add_object(pdfgen.FONT_HELVETICA)
        resources_ref = gen.add_object(pdfgen.resources_body([(b"F1", font_ref)]))
        pages_refs.append(gen.add_object(pdfgen.page_body(2, resources_ref, content_ref)))

    kids = b"".join(b"%d 0 R " % ref for ref in pages_refs)
    gen.overwrite_object(2, b"<< /Type /Pages /Kids [%s] /Count %d >>" % (kids, len(pages_refs)))

    gen.write(out_path)


if __name__ == "__main__":
    main()
