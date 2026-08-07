#!/usr/bin/env bash
# albdf performance benchmark (W8 / DB #27).
#
# Promotes the manual M7 perf smoke (scripts-tmp/m7-perf.sh) to a tracked
# benchmark. Generates a deterministic 1000-page synthetic document and
# measures the documented M7 metrics, one run per op (single-shot, matching
# the M7 methodology; process startup is included in the first op):
#   info        open + info on the 1000-page doc        (asserts 1000 pages)
#   fetch_text  fetch-text over all pages
#   render_p500 render page 500 at 72 dpi PNG           (asserts Image_500.png)
#   search      search-text "smoke" over all pages      (asserts 1000 matches)
#   delete_p500 delete-object page 500 index 0 + write  (asserts out.pdf)
#
# Machine-readable summary: an `albdf-benchmark-v1` key=value block on
# stdout (one `op=` line per metric) and in <work-dir>/benchmark-summary.txt.
# --json additionally prints the same data as a single JSON object.
#
# Threshold policy (documented in RELEASES.md + PROBLEMS.md):
#   every op must complete in <= ALBDF_PERF_THRESHOLD_MS (default 5000 ms on
#   the dev container). A slower op (or any assertion failure) marks the run
#   FAIL and exits 1. In CI this runs as a separate NON-GATING job
#   (continue-on-error: true, ci.yml `benchmark`) so a regression flags
#   itself in the uploaded summary without blocking the PR.
#
# Usage:
#   bash scripts/benchmark.sh [--binary <path>] [--work-dir <dir>]
#                             [--threshold-ms <ms>] [--json]
#   ALBDF_PERF_THRESHOLD_MS=<ms> bash scripts/benchmark.sh
#
# Requires: bash, GNU date (+%s%3N), python3 (fixture generation).
set -u

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$REPO_DIR/src/build/bin/albdf"
WORK=""
THRESHOLD_MS="${ALBDF_PERF_THRESHOLD_MS:-5000}"
JSON=0

usage() {
    sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --binary)      BIN="${2:?}"; shift 2 ;;
        --work-dir)    WORK="$2"; shift 2 ;;
        --threshold-ms) THRESHOLD_MS="$2"; shift 2 ;;
        --json)        JSON=1; shift ;;
        -h|--help)     usage; exit 0 ;;
        *) echo "benchmark: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

fail() { echo "benchmark: ERROR: $*" >&2; exit 1; }

[ -x "$BIN" ] || fail "albdf binary not found: $BIN (build first: cmake --build $REPO_DIR/src/build)"
case "$THRESHOLD_MS" in ''|*[!0-9]*) fail "threshold must be a positive integer (ms), got: '$THRESHOLD_MS'" ;; esac
[ "$THRESHOLD_MS" -gt 0 ] || fail "threshold must be > 0"

if [ -n "$WORK" ]; then mkdir -p "$WORK"; else WORK="$(mktemp -d "${TMPDIR:-/tmp}/albdf-bench.XXXXXX")"; fi
DOC="$WORK/1000pages.pdf"

# ---- deterministic 1000-page fixture (same generator as the M7 smoke) ------
echo "== generating 1000-page fixture =="
python3 - "$DOC" <<'PY' || fail "fixture generation failed"
import sys, zlib
out_path = sys.argv[1]
PAGES = 1000

def make_content(page_no):
    text = f"Page {page_no} - pdfedit perf smoke test line."
    s = f"BT /F1 12 Tf 1 0 0 1 72 700 Tm ({text}) Tj ET".encode()
    c = zlib.compress(s, 9)
    return b"<< /Length %d /Filter /FlateDecode >>\nstream\n" % len(c) + c + b"\nendstream\n"

objs = [b"<< /Type /Catalog /Pages 2 0 R >>",
        f"<< /Type /Pages /Kids [{(' '.join(f'{4+i} 0 R' for i in range(PAGES)))}] /Count {PAGES} >>".encode(),
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"]
page_objs, content_objs = [], []
for i in range(PAGES):
    page_objs.append(f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
                     f"/Resources << /Font << /F1 3 0 R >> >> /Contents {1004+i} 0 R >>".encode())
    content_objs.append(make_content(i + 1))

out = bytearray(b"%PDF-1.4\n")
offsets = [0]
all_objs = objs + page_objs + content_objs
for i, body in enumerate(all_objs, start=1):
    offsets.append(len(out))
    out += b"%d 0 obj\n" % i + body + b"\nendobj\n"

xref = len(out)
n = len(all_objs) + 1
out += b"xref\n0 %d\n" % n + b"0000000000 65535 f \n"
for off in offsets[1:]:
    out += b"%010d 00000 n \n" % off
out += b"trailer << /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (n, xref)
open(out_path, "wb").write(out)
print(f"  wrote {out_path} ({len(out)} bytes, {PAGES} pages)")
PY

# ---- timing ----------------------------------------------------------------
# time_op <label> <cmd...>  -> sets OP_MS / OP_CODE / OP_OUT
# Nanosecond timestamps (GNU date %N); ms = (t1 - t0) / 1e6. Note: %3N width
# is NOT honored by every date build, so never use it.
time_op() {
    local label="$1"; shift
    local t0 t1
    t0=$(date +%s%N)
    # Headless by default: without a display the binary aborts (exit 134).
    # CI sets this at the job level; make the script self-contained too.
    OP_OUT="$(QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" "$@" 2>&1)"; OP_CODE=$?
    t1=$(date +%s%N)
    OP_MS=$(( (t1 - t0) / 1000000 ))
}

FAILED=0
FAILED_OPS=0
ROWS=()

# record <label> <ok> [extra key=value...]
record() {
    local label="$1" ok="$2"; shift 2
    local row="op=$label,ms=$OP_MS,ok=$ok" breach=0 e
    for e in "$@"; do row="$row,$e"; done
    if [ "$OP_MS" -gt "$THRESHOLD_MS" ]; then row="$row,threshold_breach=1"; breach=1; fi
    ROWS+=("$row")
    if [ "$ok" -ne 1 ] || [ "$breach" -eq 1 ]; then FAILED=1; FAILED_OPS=$((FAILED_OPS + 1)); fi
    echo "  $label: ${OP_MS} ms (exit $OP_CODE)$([ "$ok" -eq 1 ] || echo '  ASSERTION FAILED')$([ "$breach" -eq 1 ] && echo "  > ${THRESHOLD_MS} ms THRESHOLD")"
}

echo "== info (open + info, 1000 pages) =="
time_op info "$BIN" info "$DOC"
PAGES="$(printf '%s\n' "$OP_OUT" | awk '/Page count/ {gsub(/,/, "", $3); print $3; exit}')"
if [ "$OP_CODE" -eq 0 ] && [ "$PAGES" = "1000" ]; then
    record info 1 "pages=$PAGES"
else
    record info 0 "pages=${PAGES:-?}"
fi

echo "== fetch-text (1000 pages) =="
time_op fetch_text "$BIN" fetch-text "$DOC"
if [ "$OP_CODE" -eq 0 ]; then record fetch_text 1; else record fetch_text 0; fi

echo "== render page 500 (72 dpi PNG) =="
RENDER_DIR="$WORK/render"; mkdir -p "$RENDER_DIR"
time_op render_p500 "$BIN" render "$DOC" \
    --page-first 500 --page-last 500 --image-format png \
    --image-res-dpi 72 --image-output-dir "$RENDER_DIR"
if [ "$OP_CODE" -eq 0 ] && [ -s "$RENDER_DIR/Image_500.png" ]; then
    record render_p500 1 "file=Image_500.png"
else
    record render_p500 0
fi

echo "== search 'smoke' (all pages) =="
time_op search "$BIN" search-text "$DOC" smoke
MATCHES="$(printf '%s\n' "$OP_OUT" | awk '/^[ \t]*[0-9,]+[ \t]*$/ {gsub(/,/, "", $1); print $1; exit}')"
if [ "$OP_CODE" -eq 0 ] && [ "$MATCHES" = "1000" ]; then
    record search 1 "matches=$MATCHES"
else
    record search 0 "matches=${MATCHES:-?}"
fi

echo "== delete-object page 500 index 0 (write out) =="
OUT_PDF="$WORK/out.pdf"; rm -f "$OUT_PDF"
time_op delete_p500 "$BIN" delete-object "$DOC" "$OUT_PDF" --page 500 --index 0
if [ "$OP_CODE" -eq 0 ] && [ -s "$OUT_PDF" ]; then
    record delete_p500 1 "file=out.pdf"
else
    record delete_p500 0
fi

# ---- summary ---------------------------------------------------------------
MAX_MS=0
for r in "${ROWS[@]}"; do
    ms="$(printf '%s\n' "$r" | sed -n 's/.*,ms=\([0-9]*\),.*/\1/p')"
    [ -n "$ms" ] && [ "$ms" -gt "$MAX_MS" ] && MAX_MS=$ms
done
if [ "$FAILED" -eq 0 ]; then RESULT=PASS; else RESULT=FAIL; fi

SUMMARY="$WORK/benchmark-summary.txt"
{
    echo "albdf-benchmark-v1"
    echo "binary=$BIN"
    echo "doc=1000pages"
    echo "fixture=$DOC"
    echo "threshold_ms=$THRESHOLD_MS"
    for r in "${ROWS[@]}"; do echo "$r"; done
    echo "result=$RESULT,ops=${#ROWS[@]},failed=$FAILED_OPS,max_ms=$MAX_MS"
} > "$SUMMARY"

echo
echo "== benchmark summary =="
cat "$SUMMARY"

if [ "$JSON" -eq 1 ]; then
    python3 - "$SUMMARY" <<'PY'
import json, sys
d = {"format": "albdf-benchmark-v1", "ops": []}
for line in open(sys.argv[1]):
    line = line.strip()
    if not line or line.startswith("albdf-benchmark"):
        continue
    if line.startswith("op="):
        d["ops"].append(dict(p.split("=", 1) for p in line.split(",")))
    else:
        k, v = line.split("=", 1)
        d[k] = v
print(json.dumps(d, sort_keys=True))
PY
fi

echo
if [ "$FAILED" -eq 0 ]; then
    echo "benchmark: PASS (all ops <= ${THRESHOLD_MS} ms)"
else
    echo "benchmark: FAIL (policy: every op must be <= ${THRESHOLD_MS} ms; see threshold_breach rows)"
fi
exit "$FAILED"
