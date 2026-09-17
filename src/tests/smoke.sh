#!/usr/bin/env bash
# CLI smoke test for the albdf fork.
#
# Usage: smoke.sh [albdf-binary] [fixtures-dir]
#   Defaults resolve relative to this script when arguments are omitted.
#
# Asserts invariants on the deterministic fixture corpus:
#   - version string
#   - info: page counts
#   - fetch-text: expected marker strings present
#   - render: 72 dpi PNGs produced, one per page
#   - unite: page counts add up
#   - recognize-text / delete-object: exercised when present in the binary
#     (they are being implemented in parallel; skipped cleanly until then)
#
# Exit code 0 = all checks passed.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PDFTOOL="${1:-$SCRIPT_DIR/../build/bin/albdf}"
FIXTURES="${2:-$SCRIPT_DIR/fixtures}"

PASS=0
FAIL=0

pass() { PASS=$((PASS + 1)); echo "PASS  $1"; }
fail() { FAIL=$((FAIL + 1)); echo "FAIL  $1"; }

# --- helpers ---------------------------------------------------------------

expect_exit() { # expect_exit <expected-code> <description> <cmd...>
    local expected="$1"; local desc="$2"; shift 2
    "$@" >/dev/null 2>&1
    local code=$?
    if [ "$code" -eq "$expected" ]; then pass "$desc (exit $code)"; else fail "$desc (exit $code, expected $expected)"; fi
}

page_count() { # page_count <pdf> -> prints page count from `info`
    "$PDFTOOL" info "$1" 2>/dev/null | awk '/Page count/ {print $3}'
}

# command present? `albdf <cmd> --help` prints the *command's* usage line for
# known commands. An unknown command writes its error to stderr and nothing to
# stdout (see main.cpp), so an empty first line means the command is absent.
command_present() {
    local main_line; local cmd_line
    main_line="$("$PDFTOOL" --help 2>/dev/null | head -1)"
    cmd_line="$("$PDFTOOL" "$1" --help 2>/dev/null | head -1)"
    [ -n "$main_line" ] && [ -n "$cmd_line" ] && [ "$cmd_line" != "$main_line" ]
}

# --- checks ----------------------------------------------------------------

echo "== version =="
expect_exit 0 "version exits 0" "$PDFTOOL" --version
if "$PDFTOOL" --version 2>/dev/null | grep -qE "albdf (0\.[0-9]+\.[0-9]+|1\.6\.0\.0)"; then
    pass "version string"
else
    fail "version string (got: $("$PDFTOOL" --version 2>/dev/null | head -1))"
fi

echo "== cli contract =="
# Exit-code contract (src/PdfTool/AGENT.md §5): help/version paths succeed with
# 0; unknown commands and malformed options are invalid arguments -> 7.
expect_exit 0 "version flag exits 0" "$PDFTOOL" --version
expect_exit 0 "help flag exits 0" "$PDFTOOL" --help
expect_exit 0 "help command exits 0" "$PDFTOOL" help
expect_exit 7 "unknown command exits 7" "$PDFTOOL" definitely-not-a-command
expect_exit 7 "unknown option exits 7" \
    "$PDFTOOL" info --definitely-not-an-option "$FIXTURES/test-baseline.pdf"
expect_exit 0 "add-text --help exits 0" "$PDFTOOL" add-text --help
expect_exit 0 "add-text --help-all exits 0" "$PDFTOOL" add-text --help-all
if command_present "add-text"; then
    pass "add-text --help prints command usage"
else
    fail "add-text --help prints command usage"
fi

# fixture registry: name -> pages -> expected text marker (single grep -E pattern)
declare -A PAGES=(
    [test-baseline.pdf]=2
    [multipage.pdf]=5
    [image-doc.pdf]=2
    [overlap-text.pdf]=1
)
declare -A TEXT=(
    [test-baseline.pdf]="Hello PDF4QT baseline!|Second page with numbers 12345"
    [multipage.pdf]="MULTIPAGE PAGE (ONE|TWO|THREE|FOUR|FIVE)"
    [image-doc.pdf]="EMBEDDED IMAGE DOCUMENT|IMAGE DOCUMENT SECOND PAGE"
    [overlap-text.pdf]="ROTATED OVERLAP GAMMA"
)

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

for fixture in test-baseline.pdf multipage.pdf image-doc.pdf overlap-text.pdf; do
    expected_pages="${PAGES[$fixture]}"
    pdf="$FIXTURES/$fixture"
    echo "== $fixture =="

    if [ ! -f "$pdf" ]; then fail "$fixture missing"; continue; fi

    expect_exit 0 "info $fixture" "$PDFTOOL" info "$pdf"
    actual_pages="$(page_count "$pdf")"
    if [ "$actual_pages" = "$expected_pages" ]; then
        pass "info $fixture page count $actual_pages"
    else
        fail "info $fixture page count (got '$actual_pages', expected $expected_pages)"
    fi

    expect_exit 0 "fetch-text $fixture" "$PDFTOOL" fetch-text "$pdf"
    text="$("$PDFTOOL" fetch-text "$pdf" 2>/dev/null)"
    if echo "$text" | grep -qE "${TEXT[$fixture]}"; then
        pass "fetch-text $fixture markers present"
    else
        fail "fetch-text $fixture markers missing (expected: ${TEXT[$fixture]})"
    fi

    render_dir="$WORK/render-$fixture"
    mkdir -p "$render_dir"
    expect_exit 0 "render $fixture" "$PDFTOOL" render "$pdf" \
        --page-first 1 --page-last "$expected_pages" \
        --image-format png --image-res-dpi 72 --image-output-dir "$render_dir"
    missing=""
    for k in $(seq 1 "$expected_pages"); do
        [ -s "$render_dir/Image_$k.png" ] || missing="$missing Image_$k.png"
    done
    if [ -z "$missing" ]; then
        pass "render $fixture produced $expected_pages PNG(s)"
    else
        fail "render $fixture missing/non-empty:$missing"
    fi
done

echo "== unite =="
merged="$WORK/merged.pdf"
expect_exit 0 "unite multipage+image-doc" "$PDFTOOL" unite \
    "$FIXTURES/multipage.pdf" "$FIXTURES/image-doc.pdf" "$merged"
if [ -f "$merged" ]; then
    merged_pages="$(page_count "$merged")"
    if [ "$merged_pages" = "7" ]; then
        pass "unite merged page count $merged_pages (5+2)"
    else
        fail "unite merged page count (got '$merged_pages', expected 7)"
    fi
else
    fail "unite did not produce $merged"
fi

echo "== optional commands (recognize-text, delete-object) =="
for cmd in recognize-text delete-object; do
    if command_present "$cmd"; then
        pass "$cmd present in binary"
        expect_exit 0 "$cmd --help" "$PDFTOOL" "$cmd" --help
    else
        echo "SKIP  $cmd not yet in binary (being implemented in parallel)"
    fi
done

# delete-object integration smoke: delete object 0 (a text run) on page 1 of
# the overlap fixture, then verify fetch-text no longer yields its marker.
if command_present "delete-object"; then
    DEL_OUT="$WORK/del-overlap.pdf"
    if expect_exit 0 "delete-object --page 1 --index 0" \
            "$PDFTOOL" delete-object "$FIXTURES/overlap-text.pdf" "$DEL_OUT" --page 1 --index 0; then
        if [ -f "$DEL_OUT" ]; then
            if "$PDFTOOL" fetch-text "$DEL_OUT" 2>/dev/null | grep -q "Line"; then
                fail "deleted text marker still present after delete-object"
            else
                pass "deleted text gone from fetch-text after delete-object"
            fi
        else
            fail "delete-object did not produce output document"
        fi
    fi
fi

echo
echo "== smoke summary: $PASS passed, $FAIL failed =="
[ "$FAIL" -eq 0 ]
