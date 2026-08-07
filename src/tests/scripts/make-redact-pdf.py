#!/usr/bin/env python3
"""Generate redact-annotated.pdf: 1-page deterministic fixture with two
/Subtype /Redact annotations covering two text lines (secret payloads) and
two public lines outside the redaction regions.

Purpose: `albdf redact` derives redaction regions exclusively from Redact
annotations embedded in the source PDF (PDFRedact::perform unions
AnnotationType::Redact regions; parseQuadrilaterals falls back to /Rect when
QuadPoints is absent). This fixture lets the integration test verify that the
secret lines' text is removed while public lines survive.

Regenerable: python3 make-redact-pdf.py [output.pdf]
Output is byte-deterministic (no timestamps, fixed zlib level, fixed object
order).
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pdfgen

# Text lines: (baseline y, label). 24pt Helvetica at x=72.
# Redaction rectangles below are in PDF user space (y up from bottom-left)
# and generously cover the text bounding boxes of the secret lines.
PUBLIC_LINES = [
    (720, b"PUBLIC LINE KEEPER"),
    (600, b"PUBLIC LINE OMEGA"),
]
SECRET_LINES = [
    (660, b"TOP SECRET PAYLOAD ALPHA", b"[60 630 400 685]"),
    (540, b"CLASSIFIED BETA BLOCK", b"[60 510 400 565]"),
]


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "redact-annotated.pdf"
    gen = pdfgen.PdfGen()

    # Object layout: 1 = Catalog, 2 = Pages tree (placeholder, filled below),
    # 3 = contents stream, 4 = font, 5 = resources, 6 = page (with /Annots),
    # 7..8 = Redact annotations.
    gen.add_object(b"<< /Type /Catalog /Pages 2 0 R >>")
    gen.add_object(b"")  # reserved slot for the /Pages tree (object 2)

    ops = []
    for y, label in PUBLIC_LINES:
        ops.append(b"BT /F1 24 Tf 72 %d Td (%s) Tj ET\n" % (y, label))
    for y, label, _rect in SECRET_LINES:
        ops.append(b"BT /F1 24 Tf 72 %d Td (%s) Tj ET\n" % (y, label))
    content = b"".join(ops)
    content_ref = gen.add_object(gen.stream(content))
    font_ref = gen.add_object(pdfgen.FONT_HELVETICA)
    resources_ref = gen.add_object(pdfgen.resources_body([(b"F1", font_ref)]))

    annotation_refs = []
    for _y, _label, rect in SECRET_LINES:
        annotation_refs.append(
            gen.add_object(b"<< /Type /Annot /Subtype /Redact /Rect %s >>" % rect)
        )
    annots = b" ".join(b"%d 0 R" % ref for ref in annotation_refs)

    page_body = (
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        b"/Resources %d 0 R /Contents %d 0 R /Annots [%s] >>"
        % (resources_ref, content_ref, annots)
    )
    page_ref = gen.add_object(page_body)

    gen.overwrite_object(2, b"<< /Type /Pages /Kids [%d 0 R] /Count 1 >>" % page_ref)

    gen.write(out_path)


if __name__ == "__main__":
    main()
