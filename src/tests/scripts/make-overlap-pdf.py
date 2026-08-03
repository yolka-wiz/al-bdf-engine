#!/usr/bin/env python3
"""Generate overlap-text.pdf: 1-page deterministic fixture with overlapping
text: two lines at the same baseline position in different colors, plus a
third line rotated 30 degrees across the same area.

Regenerable: python3 make-overlap-pdf.py [output.pdf]
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pdfgen


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "overlap-text.pdf"
    gen = pdfgen.PdfGen()

    gen.add_object(b"<< /Type /Catalog /Pages 2 0 R >>")
    gen.add_object(b"")

    font_ref = gen.add_object(pdfgen.FONT_HELVETICA)

    # Line 1 (red) and line 2 (blue) share the exact same baseline position,
    # so they fully overlap. Line 3 is rotated 30 degrees around (72, 620).
    content = (b"BT /F1 24 Tf 0.8 0.1 0.1 rg 72 700 Td (OVERLAPPING TEXT LINE ALPHA) Tj ET\n"
               b"BT /F1 24 Tf 0.1 0.1 0.8 rg 72 700 Td (overlapping text line beta) Tj ET\n"
               b"BT /F1 18 Tf 0.1 0.6 0.2 rg 0.866 0.5 -0.5 0.866 0 0 cm "
               b"72 620 Td (ROTATED OVERLAP GAMMA) Tj ET\n")
    content_ref = gen.add_object(gen.stream(content))
    res_ref = gen.add_object(pdfgen.resources_body([(b"F1", font_ref)]))
    page_ref = gen.add_object(pdfgen.page_body(2, res_ref, content_ref))

    gen.overwrite_object(2, b"<< /Type /Pages /Kids [%d 0 R] /Count 1 >>" % page_ref)

    gen.write(out_path)


if __name__ == "__main__":
    main()
