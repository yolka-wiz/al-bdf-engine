#!/usr/bin/env python3
"""Deterministic minimal PDF writer for the pdfedit test fixture corpus.

Byte-deterministic by construction: no timestamps, fixed object order,
fixed zlib level (6). Generalizes the pattern from
scripts-tmp/make-test-pdf.py (computes xref offsets properly).

Usage: import pdfgen, build a document, call write() with an output path.
"""

import zlib

FONT_HELVETICA = b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"


class PdfGen:
    """Accumulates PDF objects and serializes a well-formed PDF 1.4 file."""

    def __init__(self):
        self._objects = []  # object bodies (bytes), index+1 == object number

    def add_object(self, body: bytes) -> int:
        """Append an object body; returns its 1-based object number."""
        self._objects.append(body)
        return len(self._objects)

    def overwrite_object(self, index: int, body: bytes) -> None:
        """Replace the body of an already-allocated object (1-based index).
        Used to reserve the /Pages tree slot before page refs exist."""
        self._objects[index - 1] = body

    def stream(self, data: bytes, extra: bytes = b"", flate: bool = False) -> bytes:
        """Build a stream object body. With flate=True the data is zlib
        compressed (deterministic) and /Filter /FlateDecode is emitted."""
        if flate:
            payload = zlib.compress(data)
            header = b"<< /Length %d /Filter /FlateDecode %s >>\nstream\n" % (len(payload), extra)
            return header + payload + b"\nendstream"
        header = b"<< /Length %d %s >>\nstream\n" % (len(data), extra)
        return header + data + b"endstream"

    def build(self) -> bytes:
        out = bytearray(b"%PDF-1.4\n")
        offsets = [0]
        for i, body in enumerate(self._objects, start=1):
            offsets.append(len(out))
            out += b"%d 0 obj\n" % i + body + b"\nendobj\n"
        xref_pos = len(out)
        n = len(self._objects) + 1
        out += b"xref\n0 %d\n" % n
        out += b"0000000000 65535 f \n"
        for off in offsets[1:]:
            out += b"%010d 00000 n \n" % off
        out += b"trailer << /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (n, xref_pos)
        return bytes(out)

    def write(self, path: str) -> None:
        data = self.build()
        with open(path, "wb") as f:
            f.write(data)
        print("wrote %s: %d bytes, %d objects" % (path, len(data), len(self._objects)))


def page_body(parent_ref: int, resources_ref: int, contents_ref: int,
              mediabox: bytes = b"[0 0 612 792]") -> bytes:
    return (b"<< /Type /Page /Parent %d 0 R /MediaBox %s /Resources %d 0 R /Contents %d 0 R >>"
            % (parent_ref, mediabox, resources_ref, contents_ref))


def resources_body(fonts: list, xobjects: list = None) -> bytes:
    """fonts: list of (name, ref) e.g. [(b'F1', 5)]. xobjects: list of (name, ref)."""
    font_part = b"".join(b"/%s %d 0 R " % (name, ref) for name, ref in fonts)
    body = b"<< /Font << %s >>" % font_part
    if xobjects:
        xo_part = b"".join(b"/%s %d 0 R " % (name, ref) for name, ref in xobjects)
        body += b" /XObject << %s >>" % xo_part
    body += b" >>"
    return body


def image_xobject_dict(width: int, height: int, extra: bytes = b"") -> bytes:
    """Stream-dict fragment for an image XObject with raw RGB samples
    (/Filter /FlateDecode is added by PdfGen.stream(flate=True))."""
    return (b"/Type /XObject /Subtype /Image /Width %d /Height %d "
            b"/ColorSpace /DeviceRGB /BitsPerComponent 8 %s" % (width, height, extra))
