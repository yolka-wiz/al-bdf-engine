#!/usr/bin/env bash
# albdf CLI fuzz harness (M10 wave-2 / DB #30).
#
# Deterministic, headless input fuzzing of the albdf CLI. Builds a corpus of
# valid PDFs (the committed byte-deterministic fixtures, regenerated via
# src/tests/scripts/make-*.pdf.py), derives seeded mutations (truncation,
# zero-fill, byte flips, garbage headers, empty files, junk-only files,
# concatenation, prepended/append garbage), then runs every key CLI command
# against each mutation with a per-invocation timeout. Also exercises the
# argument-parsing path with oversized/negative/empty CLI args on valid docs.
#
# Crash policy (see src/AGENT.md exit-code contract): only a signal death
# (exit >= 128: SEGV/ABRT/... = CRASH) or a timeout kill (exit 124/137 =
# HANG) is a finding. Nonzero exits on corrupt input are legitimate contract
# behavior (e.g. 4 = ErrorDocumentReading, 7 = ErrorInvalidArguments) and
# are recorded, not failed. Harness errors (missing binary, timeout(1)
# failures) fail the run.
#
# The run CONTINUES after crashes so every case is exercised and all findings
# are collected in one pass (crashes are fast). Hangs cost the full
# per-invocation timeout, so the run stops once --max-hangs hangs have been
# recorded (reproducers are saved first); harness errors also stop the run
# immediately since every later case would fail the same way.
#
# Determinism: mutations come from python3's random.Random(seed), which is
# stable across runs/platforms for a fixed seed; the corpus, case order, and
# summary contain no timestamps. Same seed -> same corpus -> same summary.
#
# Machine-readable summary: an `albdf-fuzz-v1` key=value block on stdout and
# in <work-dir>/fuzz-summary.txt (mirrors scripts/benchmark.sh conventions).
# Per-case detail (case, cmd, input, exit, class) goes to
# <work-dir>/fuzz-cases.tsv. Failing inputs + exact repro command lines are
# saved under <work-dir>/fail/ and the work-dir is retained on failure.
#
# Exit codes: 0 = no crashes/hangs (PASS); 1 = crash/hang/harness error
# (FAIL); 2 = bad usage.
#
# Usage:
#   bash scripts/fuzz.sh [--seed N] [--iterations N] [--binary <path>]
#                        [--work-dir <dir>] [--timeout <sec>]
#                        [--max-time <sec>] [--max-hangs N] [--keep]
#   ALBDF_FUZZ_SEED=N ALBDF_FUZZ_ITERATIONS=N ALBDF_FUZZ_TIMEOUT=N bash scripts/fuzz.sh
#
# Requires: bash, GNU timeout(1), python3 (corpus/mutation generation),
# and a built albdf binary (default src/build/bin/albdf).
set -u

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$REPO_DIR/src/build/bin/albdf"
FIXTURES="$REPO_DIR/src/tests/fixtures"
GEN_DIR="$REPO_DIR/src/tests/scripts"
SEED="${ALBDF_FUZZ_SEED:-20260806}"
ITER="${ALBDF_FUZZ_ITERATIONS:-50}"
TIMEOUT_SEC="${ALBDF_FUZZ_TIMEOUT:-20}"
MAX_TIME=0
MAX_HANGS=3
WORK=""
EXPLICIT_WORK=0
KEEP=0
FAILED=0

usage() {
    sed -n '2,42p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --seed)        SEED="${2:?}"; shift 2 ;;
        --iterations)  ITER="${2:?}"; shift 2 ;;
        --binary)      BIN="${2:?}"; shift 2 ;;
        --work-dir)    WORK="$2"; EXPLICIT_WORK=1; shift 2 ;;
        --timeout)     TIMEOUT_SEC="${2:?}"; shift 2 ;;
        --max-time)    MAX_TIME="${2:?}"; shift 2 ;;
        --max-hangs)   MAX_HANGS="${2:?}"; shift 2 ;;
        --keep)        KEEP=1; shift ;;
        -h|--help)     usage; exit 0 ;;
        *) echo "fuzz: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

fail() { echo "fuzz: ERROR: $*" >&2; exit 1; }

case "$SEED" in ''|*[!0-9]*) fail "seed must be a non-negative integer, got: '$SEED'" ;; esac
case "$ITER" in ''|*[!0-9]*) fail "iterations must be a positive integer, got: '$ITER'" ;; esac
[ "$ITER" -gt 0 ] || fail "iterations must be > 0"
case "$TIMEOUT_SEC" in ''|*[!0-9]*) fail "timeout must be a positive integer (seconds), got: '$TIMEOUT_SEC'" ;; esac
[ "$TIMEOUT_SEC" -gt 0 ] || fail "timeout must be > 0"
case "$MAX_TIME" in ''|*[!0-9]*) fail "max-time must be a positive integer (seconds), got: '$MAX_TIME'" ;; esac
case "$MAX_HANGS" in ''|*[!0-9]*) fail "max-hangs must be a positive integer, got: '$MAX_HANGS'" ;; esac
[ "$MAX_HANGS" -gt 0 ] || fail "max-hangs must be > 0"

[ -x "$BIN" ] || fail "albdf binary not found: $BIN (build first: cmake --build $REPO_DIR/src/build)"
command -v timeout >/dev/null 2>&1 || fail "timeout(1) required (GNU coreutils)"
command -v python3 >/dev/null 2>&1 || fail "python3 required"
[ -f "$GEN_DIR/make-multipage-pdf.py" ] || fail "fixture generators not found in $GEN_DIR"
[ -f "$FIXTURES/blank.pdf" ] || fail "committed fixture not found: $FIXTURES/blank.pdf"
[ -f "$FIXTURES/test-baseline.pdf" ] || fail "committed fixture not found: $FIXTURES/test-baseline.pdf"

# Headless everywhere: no display, deterministic fontconfig-less rendering.
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"

if [ -n "$WORK" ]; then mkdir -p "$WORK"; else WORK="$(mktemp -d "${TMPDIR:-/tmp}/albdf-fuzz.XXXXXX")"; fi
mkdir -p "$WORK/base" "$WORK/corpus" "$WORK/out" "$WORK/logs" "$WORK/fail"
DOC="$WORK/base/multipage.pdf"          # valid doc for the CLI-arg cases
BLANK="$FIXTURES/blank.pdf"             # valid 1-page doc for add-text cases
FONT_FA="$REPO_DIR/src/tests/fonts/NotoNaskhArabic-Regular.ttf"
RENDIR="$WORK/out/render"; mkdir -p "$RENDIR"

cleanup() {
    if [ "$FAILED" -eq 1 ] || [ "$KEEP" -eq 1 ] || [ "$EXPLICIT_WORK" -eq 1 ]; then
        [ "$FAILED" -eq 1 ] && echo "fuzz: FAIL — work-dir kept with reproducers: $WORK (failing inputs in $WORK/fail/)" >&2
    else
        rm -rf "$WORK"
    fi
}
trap cleanup EXIT

# ---- base corpus (deterministic generators + committed fixtures) ----------
echo "== generating base corpus =="
python3 "$GEN_DIR/make-multipage-pdf.py" "$WORK/base/multipage.pdf" >/dev/null || fail "make-multipage-pdf.py failed"
python3 "$GEN_DIR/make-image-pdf.py"     "$WORK/base/image-doc.pdf"   >/dev/null || fail "make-image-pdf.py failed"
python3 "$GEN_DIR/make-overlap-pdf.py"   "$WORK/base/overlap-text.pdf" >/dev/null || fail "make-overlap-pdf.py failed"
cp "$FIXTURES/blank.pdf"         "$WORK/base/blank.pdf"
cp "$FIXTURES/test-baseline.pdf" "$WORK/base/test-baseline.pdf"

# Sanity check: generators must reproduce the committed fixtures byte-for-byte
# (the corpus is only deterministic if they do; a mismatch is a warning, not
# a failure).
GEN_REPRO=yes
for f in multipage image-doc overlap-text; do
    cmp -s "$WORK/base/$f.pdf" "$FIXTURES/$f.pdf" || GEN_REPRO=no
done

# ---- seeded mutation manifest ----------------------------------------------
echo "== generating $ITER mutations (seed $SEED) =="
python3 - "$SEED" "$ITER" "$WORK" <<'PY' || fail "mutation generation failed"
import os
import random
import sys

seed, iterations, work = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
rng = random.Random(seed)
bases = ["multipage", "image-doc", "overlap-text", "blank", "test-baseline"]
HEADERS = [b"%!PS-Adobe-3.0", b"GIF89a", b"\x89PNG\r\n\x1a\n", b"PK\x03\x04", b"MZ\x90\x00"]

manifest = []
for i in range(iterations):
    base = bases[i % len(bases)]
    with open(os.path.join(work, "base", base + ".pdf"), "rb") as f:
        data = f.read()
    n = len(data)
    t = rng.randrange(10)
    if t == 0:  # truncate at a deterministic offset (0 .. full size)
        cut = rng.randrange(0, n + 1)
        out, meta = data[:cut], "trunc=%d" % cut
    elif t == 1:  # zero-fill a contiguous region
        out = bytearray(data)
        if n:
            start = rng.randrange(0, n)
            ln = rng.randrange(1, min(4096, n - start) + 1)
            out[start:start + ln] = b"\x00" * ln
            meta = "zerofill=%d+%d" % (start, ln)
        else:
            meta = "zerofill=empty"
        out = bytes(out)
    elif t == 2:  # 1..8 random bit flips
        out = bytearray(data)
        k = rng.randrange(1, 9)
        for _ in range(k):
            if not out:
                break
            off = rng.randrange(0, len(out))
            out[off] ^= (1 << rng.randrange(8))
        out, meta = bytes(out), "flips=%d" % k
    elif t == 3:  # overwrite the header with garbage (or another file magic)
        w = min(rng.randrange(4, 65), n) if n else 0
        if w and rng.randrange(2):
            h = rng.choice(HEADERS)
            hdr = (h * ((w // len(h)) + 1))[:w]
        else:
            hdr = bytes(rng.randrange(256) for _ in range(w))
        out, meta = hdr + data[w:], "header=%d" % w
    elif t == 4:  # append garbage after the %%EOF
        tail = bytes(rng.randrange(256) for _ in range(rng.randrange(1, 4097)))
        out, meta = data + tail, "append=%d" % len(tail)
    elif t == 5:  # xor a contiguous region with a fixed key
        out = bytearray(data)
        if n:
            start = rng.randrange(0, n)
            ln = min(rng.randrange(1, 4097), n - start)
            key = rng.randrange(1, 256)
            for j in range(start, start + ln):
                out[j] ^= key
            meta = "xor=%d+%d+%d" % (start, ln, key)
        else:
            meta = "xor=empty"
        out = bytes(out)
    elif t == 6:  # empty file
        out, meta = b"", "empty"
    elif t == 7:  # junk-only file (no PDF at all)
        out, meta = bytes(rng.randrange(256) for _ in range(rng.randrange(8, 2049))), "garbage=%d" % n
    elif t == 8:  # file concatenated with itself
        out, meta = data + data, "concat"
    else:  # t == 9: prepend junk before the %PDF header
        pre = bytes(rng.randrange(256) for _ in range(rng.randrange(1, 65)))
        out, meta = pre + data, "prepend=%d" % len(pre)

    path = os.path.join(work, "corpus", "m%d.pdf" % i)
    with open(path, "wb") as f:
        f.write(out)
    manifest.append((i, path, base, t, meta))

with open(os.path.join(work, "manifest.tsv"), "w") as f:
    for row in manifest:
        f.write("\t".join(str(x) for x in row) + "\n")
print("  wrote %d mutations (%d bytes)" % (len(manifest), sum(os.path.getsize(r[1]) for r in manifest)))
PY
[ -s "$WORK/manifest.tsv" ] || fail "mutation manifest is empty"

# ---- case runner ------------------------------------------------------------
# State shared with run_one (globals, not subshells):
declare -A HIST=()
CRASHES=0; HANGS=0; HARNESS_ERRS=0; UNEXPECTED=0; OPS=0; STOP=0; MAXTIME_HIT=0
FAILURES=()
: > "$WORK/fuzz-cases.tsv"
START_TS="$(date +%s)"

# run_one <case-id> <cmd-name> <allowed> <input> <argv...>
#   allowed == "*"   -> any non-crash exit is expected (corrupt-input contract)
#   allowed == "0 4 7" -> only those exit codes are expected (CLI-arg cases)
# Crashes are recorded with reproducers and the run continues; STOP is set
# only on a harness ERROR (every later case would fail the same way) or when
# --max-hangs hangs have been recorded (each hang costs a full timeout).
run_one() {
    local id="$1" cmd="$2" allowed="$3" input="$4"; shift 4
    local rc cls ok c
    timeout --kill-after=5 "$TIMEOUT_SEC" "$BIN" "$@" >"$WORK/logs/$id.log" 2>&1
    rc=$?
    OPS=$((OPS + 1))
    HIST[$rc]=$(( ${HIST[$rc]:-0} + 1 ))
    cls=OK
    if [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ]; then
        cls=HANG; HANGS=$((HANGS + 1))
    elif [ "$rc" -ge 128 ]; then
        cls=CRASH; CRASHES=$((CRASHES + 1))
    elif [ "$rc" -eq 125 ] || [ "$rc" -eq 126 ] || [ "$rc" -eq 127 ]; then
        cls=ERROR; HARNESS_ERRS=$((HARNESS_ERRS + 1))
    elif [ "$allowed" != "*" ]; then
        ok=0
        for c in $allowed; do [ "$rc" -eq "$c" ] && ok=1; done
        if [ "$ok" -eq 0 ]; then cls=UNEXPECTED; UNEXPECTED=$((UNEXPECTED + 1)); fi
    fi
    printf '%s\t%s\t%s\t%s\t%s\n' "$id" "$cmd" "$input" "$rc" "$cls" >> "$WORK/fuzz-cases.tsv"
    if [ "$cls" = CRASH ] || [ "$cls" = HANG ]; then
        echo "  FAIL $id: $cls (exit $rc) — input: $input"
        cp "$input" "$WORK/fail/$id.pdf" 2>/dev/null
        cp "$WORK/logs/$id.log" "$WORK/fail/$id.log" 2>/dev/null
        { printf '%q ' "$BIN" "$@"; echo; } > "$WORK/fail/$id.cmd"
        # Keep summary paths relative to the work-dir so summaries are
        # byte-identical across runs (determinism gate).
        FAILURES+=("$id: class=$cls exit=$rc input=${input#"$WORK"/} repro=fail/$id.pdf")
        if [ "$cls" = HANG ] && [ "$HANGS" -ge "$MAX_HANGS" ]; then
            echo "  fuzz: hit --max-hangs $MAX_HANGS, stopping (remaining cases skipped)"
            STOP=1
        fi
    elif [ "$cls" = ERROR ]; then
        echo "  FAIL $id: HARNESS ERROR (exit $rc) — cmd could not run; aborting"
        FAILURES+=("$id: class=ERROR exit=$rc input=$input")
        STOP=1
    elif [ "$cls" = UNEXPECTED ]; then
        echo "  note $id: unexpected exit $rc (allowed: '$allowed') — cmd: $cmd"
    fi
}

# build_argv <cmd> <input> <out> <rendir>  ->  sets AVG
build_argv() {
    local cmd="$1" input="$2" out="$3" rendir="$4"
    case "$cmd" in
        info)          AVG=(info "$input") ;;
        fetch-text)    AVG=(fetch-text "$input") ;;
        search-text)   AVG=(search-text "$input" "MULTIPAGE") ;;
        add-text)      AVG=(add-text "$input" "$out" --page 1 --x 72 --y 72 --text "FUZZ") ;;
        delete-object) AVG=(delete-object "$input" "$out" --page 1 --index 0) ;;
        delete-page)   AVG=(delete-page "$input" "$out" --page 1) ;;
        rotate)        AVG=(rotate "$input" "$out" --page 1 --angle 90) ;;
        move-page)     AVG=(move-page "$input" "$out" --from 1 --to 2) ;;
        redact)        AVG=(redact "$input" "$out") ;;
        render)        AVG=(render "$input" --page-first 1 --page-last 1 \
                            --image-format png --image-res-dpi 72 --image-output-dir "$rendir") ;;
        *) fail "internal: unknown command '$cmd'" ;;
    esac
}

# ---- CLI-arg cases (valid docs + abusive/oversized/empty args) --------------
echo "== CLI argument fuzz cases (valid docs) =="
cli_case() { # cli_case <id> <allowed>   (sets AVG; input defaults to $DOC)
    local id="$1" allowed="$2" input="$DOC" cmd
    case "$id" in
        cli_no_args)          AVG=() ;;
        cli_unknown_cmd)      AVG=(frobnicate "$DOC") ;;
        cli_info_no_doc)      AVG=(info) ;;
        cli_delobj_huge_page) AVG=(delete-object "$DOC" "$WORK/out/$id.pdf" --page 999999999 --index 0) ;;
        cli_delobj_neg_page)  AVG=(delete-object "$DOC" "$WORK/out/$id.pdf" --page -1 --index 0) ;;
        cli_delobj_huge_idx)  AVG=(delete-object "$DOC" "$WORK/out/$id.pdf" --page 1 --index 999999999) ;;
        cli_rotate_bad_angle) AVG=(rotate "$DOC" "$WORK/out/$id.pdf" --page 1 --angle 45) ;;
        cli_rotate_zero)      AVG=(rotate "$DOC" "$WORK/out/$id.pdf" --page 1 --angle 0) ;;
        cli_rotate_huge_page) AVG=(rotate "$DOC" "$WORK/out/$id.pdf" --page 999999999 --angle 90) ;;
        cli_move_huge_from)   AVG=(move-page "$DOC" "$WORK/out/$id.pdf" --from 999999999 --to 1) ;;
        cli_move_huge_to)     AVG=(move-page "$DOC" "$WORK/out/$id.pdf" --from 1 --to 999999999) ;;
        cli_delpage_huge)     AVG=(delete-page "$DOC" "$WORK/out/$id.pdf" --page 999999999) ;;
        cli_delpage_badsel)   AVG=(delete-page "$DOC" "$WORK/out/$id.pdf" --page-select "abc") ;;
        cli_delpage_hugesel)  AVG=(delete-page "$DOC" "$WORK/out/$id.pdf" --page-select "1-999999999") ;;
        cli_delpage_all)      AVG=(delete-page "$DOC" "$WORK/out/$id.pdf" --page-select "1-5") ;;
        cli_addtext_huge_pg)  AVG=(add-text "$DOC" "$WORK/out/$id.pdf" --page 999999999 --x 72 --y 72 --text FUZZ) ;;
        cli_addtext_neg_crd)  AVG=(add-text "$DOC" "$WORK/out/$id.pdf" --page 1 --x -5000 --y -5000 --size 99999 --text FUZZ) ;;
        cli_addtext_empty)    AVG=(add-text "$DOC" "$WORK/out/$id.pdf" --page 1 --x 72 --y 72 --text "") ;;
        cli_search_empty_q)   AVG=(search-text "$DOC" "") ;;
        cli_render_page0)     AVG=(render "$DOC" --page-first 0 --page-last 1 \
                                    --image-format png --image-res-dpi 72 --image-output-dir "$RENDIR") ;;
        cli_render_huge_last) AVG=(render "$DOC" --page-first 1 --page-last 999999999 \
                                    --image-format png --image-res-dpi 72 --image-output-dir "$RENDIR") ;;
        cli_render_huge_dpi)  AVG=(render "$DOC" --page-first 1 --page-last 1 \
                                    --image-format png --image-res-dpi 999999 --image-output-dir "$RENDIR") ;;
        cli_render_neg_first) AVG=(render "$DOC" --page-first -1 --page-last 1 \
                                    --image-format png --image-res-dpi 72 --image-output-dir "$RENDIR") ;;
        cli_render_bad_fmt)   AVG=(render "$DOC" --page-first 1 --page-last 1 \
                                    --image-format bogus --image-res-dpi 72 --image-output-dir "$RENDIR") ;;
        cli_bad_console_fmt)  AVG=(info "$DOC" --console-format bogus) ;;
        cli_rtl_addtext)      input="$BLANK"; AVG=(add-text "$input" "$WORK/out/$id.pdf" \
                                    --page 1 --x 72 --y 72 --text "سلام" --lang fa --font "$FONT_FA") ;;
        cli_rtl_search)       AVG=(search-text "$DOC" "سلام") ;;
        *) fail "internal: unknown cli case '$id'" ;;
    esac
    cmd="${AVG[0]:-none}"
    run_one "$id" "$cmd" "$allowed" "$input" "${AVG[@]}"
}

CLI_IDS=(cli_no_args cli_unknown_cmd cli_info_no_doc \
    cli_delobj_huge_page cli_delobj_neg_page cli_delobj_huge_idx \
    cli_rotate_bad_angle cli_rotate_zero cli_rotate_huge_page \
    cli_move_huge_from cli_move_huge_to \
    cli_delpage_huge cli_delpage_badsel cli_delpage_hugesel cli_delpage_all \
    cli_addtext_huge_pg cli_addtext_neg_crd cli_addtext_empty \
    cli_search_empty_q cli_render_page0 cli_render_huge_last cli_render_huge_dpi \
    cli_render_neg_first cli_render_bad_fmt cli_bad_console_fmt \
    cli_rtl_addtext cli_rtl_search)
for id in "${CLI_IDS[@]}"; do
    [ "$STOP" -eq 1 ] && break
    cli_case "$id" "0 4 7"
done

# ---- mutation cases ----------------------------------------------------------
echo "== fuzzing mutations (all commands, per-invocation timeout ${TIMEOUT_SEC}s) =="
CMDS=(info fetch-text search-text add-text delete-object delete-page rotate move-page redact render)
while IFS=$'\t' read -r idx path base t meta; do
    [ "$STOP" -eq 1 ] && break
    if [ "$MAX_TIME" -gt 0 ] && [ $(( $(date +%s) - START_TS )) -ge "$MAX_TIME" ]; then
        MAXTIME_HIT=1; break
    fi
    for cmd in "${CMDS[@]}"; do
        [ "$STOP" -eq 1 ] && break
        if [ "$MAX_TIME" -gt 0 ] && [ $(( $(date +%s) - START_TS )) -ge "$MAX_TIME" ]; then
            MAXTIME_HIT=1; break
        fi
        build_argv "$cmd" "$path" "$WORK/out/m${idx}_${cmd}.pdf" "$RENDIR"
        run_one "m${idx}_${cmd}" "$cmd" "*" "$path" "${AVG[@]}"
    done
done < "$WORK/manifest.tsv"

# ---- summary ----------------------------------------------------------------
PLANNED=$(( ITER * ${#CMDS[@]} + ${#CLI_IDS[@]} ))
SKIPPED=$(( PLANNED - OPS ))
[ "$SKIPPED" -lt 0 ] && SKIPPED=0
[ "$CRASHES" -gt 0 ] && FAILED=1
[ "$HANGS" -gt 0 ] && FAILED=1
[ "$HARNESS_ERRS" -gt 0 ] && FAILED=1
if [ "$FAILED" -eq 0 ]; then RESULT=PASS; else RESULT=FAIL; fi

HIST_LINE=""
for c in $(printf '%s\n' "${!HIST[@]}" | sort -n); do
    HIST_LINE="${HIST_LINE:+$HIST_LINE,}$c:${HIST[$c]}"
done

SUMMARY="$WORK/fuzz-summary.txt"
{
    echo "albdf-fuzz-v1"
    echo "binary=$BIN"
    echo "seed=$SEED"
    echo "iterations=$ITER"
    echo "timeout_sec=$TIMEOUT_SEC"
    echo "max_time_sec=$MAX_TIME"
    echo "max_hangs=$MAX_HANGS"
    echo "base_docs=5"
    echo "mutation_types=10"
    echo "cli_cases=${#CLI_IDS[@]}"
    echo "commands=${#CMDS[@]}"
    echo "ops=$OPS"
    echo "skipped=$SKIPPED"
    echo "crashes=$CRASHES"
    echo "hangs=$HANGS"
    echo "harness_errors=$HARNESS_ERRS"
    echo "unexpected_exits=$UNEXPECTED"
    echo "generator_reproducible=$GEN_REPRO"
    echo "exit_histogram=${HIST_LINE:-none}"
    echo "failures=${#FAILURES[@]}"
    for f in "${FAILURES[@]}"; do echo "failure=$f"; done
    if [ "$MAXTIME_HIT" -eq 1 ]; then echo "note=stopped early: max-time $MAX_TIME s budget exhausted ($SKIPPED cases skipped)"; fi
    if [ "$STOP" -eq 1 ] && [ "$MAXTIME_HIT" -eq 0 ] && [ "$HARNESS_ERRS" -eq 0 ]; then
        echo "note=stopped early: max-hangs $MAX_HANGS reached ($SKIPPED cases skipped)"
    fi
    echo "result=$RESULT"
} > "$SUMMARY"

echo
echo "== fuzz summary =="
cat "$SUMMARY"

echo
if [ "$FAILED" -eq 0 ]; then
    echo "fuzz: PASS (no crashes/hangs in $OPS ops, seed $SEED)"
else
    echo "fuzz: FAIL (see failure= lines above; reproducers in $WORK/fail/)"
fi
exit "$FAILED"
