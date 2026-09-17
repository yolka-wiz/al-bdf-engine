#!/usr/bin/env bash
# albdf anti-slop gate (roadmap R2.3; rules: docs/coding-standard.md §11).
#
# Enforces the mechanically checkable §11.8 rules on changed authored files:
#   S6  frozen files may not grow: src/PdfTool/pdftoolabstractapplication.{h,cpp}
#   C3  TODO/FIXME/HACK/XXX must carry a DB ref, e.g. // TODO(#123): ...
#   E1  no Q_ASSERT(/assert( in changed files under src/PdfTool/**
#   U3  newly added authored .cpp/.h carry SPDX-License-Identifier: GPL-3.0-or-later
# Advisory (warn-only, never blocks):
#   U2  edits to vendored upstream files should be sync(upstream): commits
#   S7  raw new/delete in changed authored files
#
# Scope is diff-scoped by default (files changed vs origin/main, falling back
# to the fork base 6bf5047) — like the clang-format gate. --all scans the whole
# repo and is advisory (grandfathered violations exist, so it is not gating).
#
# Usage:
#   scripts/check-slop.sh [--all | --staged | --base <ref>] [--self-test] [-h]
#
#   --all          scan every authored source file (advisory)
#   --staged       scan files staged for commit (used by .githooks/pre-commit)
#   --base <ref>   diff against <ref> (default: origin/main, else 6bf5047)
#   --self-test    run the built-in fixture suite and exit
#   -h, --help     show this help
#
# Output: deterministic PASS/FAIL/WARN lines + a summary. Exit 0 = no FAIL;
# 1 = at least one FAIL; 2 = usage/environment error. WARN never fails.
#
# Requires: bash, awk, git, grep, sort, wc, mktemp. No network, no build.
set -u

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FORK_BASE="6bf5047"

# Frozen-file baselines (S6). Measured on main @ 5a87f99 (2026-09-17). These
# are the CURRENT counts, not a budget: the files may shrink but not grow
# until the R3 CLI decomposition lands (ADR-0007, PLAN §3.2).
FROZEN_PDFTOOLAPPLICATION_H=472
FROZEN_PDFTOOLAPPLICATION_CPP=1812

MODE="diff"
BASE_REF=""
SELF_TEST=0
TOTAL_FAIL=0
TOTAL_WARN=0

# Vendored upstream paths (U2), mirroring ci/run-ci.sh's authored-file
# exclusions (same cherry-pick-hygiene set).
VENDORED_DIRS="
src/Pdf4QtLibGui
src/Pdf4QtLibWidgets
src/Pdf4QtEditor
src/Pdf4QtViewer
src/Pdf4QtPageMaster
src/vcpkg
"
VENDORED_FILES="
src/PdfTool/pdftoolabstractapplication.h
src/PdfTool/pdftoolabstractapplication.cpp
src/PdfTool/main.cpp
src/PdfTool/pdftoolrender.cpp
src/Pdf4QtLibCore/sources/pdfpagecontenteditorprocessor.h
src/Pdf4QtLibCore/sources/pdfpagecontenteditorprocessor.cpp
src/Pdf4QtLibCore/sources/pdfpagecontenteditorcontentstreambuilder.h
src/Pdf4QtLibCore/sources/pdfpagecontenteditorcontentstreambuilder.cpp
src/Pdf4QtLibCore/sources/pdftextlayout.cpp
src/Pdf4QtLibCore/sources/pdfdocumentbuilder.h
src/Pdf4QtLibCore/sources/pdfdocumentbuilder.cpp
src/Pdf4QtLibCore/sources/pdffont.cpp
src/Pdf4QtLibCore/sources/pdfpagecontentprocessor.h
src/Pdf4QtLibCore/sources/pdfpagecontentprocessor.cpp
src/Pdf4QtLibCore/sources/pdfutils.h
src/Pdf4QtLibCore/sources/pdfutils.cpp
"

usage() {
    sed -n '/^# Usage:/,/^# Output:/p' "$0" | sed 's/^# \{0,1\}//' | sed '$d'
}

display_path() {
    case "$1" in
        "$REPO_DIR"/*) printf '%s' "${1#"$REPO_DIR"/}" ;;
        *) printf '%s' "$1" ;;
    esac
}

is_vendored() {
    local f="$1" d v
    for d in $VENDORED_DIRS; do
        case "$f" in "$d"/*) return 0 ;; esac
    done
    for v in $VENDORED_FILES; do
        [ "$f" = "$v" ] && return 0
    done
    return 1
}

resolve_base() {
    if git -C "$REPO_DIR" rev-parse --verify --quiet origin/main >/dev/null 2>&1; then
        printf 'origin/main'
    elif git -C "$REPO_DIR" rev-parse --verify --quiet "$FORK_BASE" >/dev/null 2>&1; then
        printf '%s' "$FORK_BASE"
    fi
}

collect_changed() {
    case "$MODE" in
        all)
            git -C "$REPO_DIR" ls-files -- '*.cpp' '*.h'
            ;;
        staged)
            git -C "$REPO_DIR" diff --cached --name-only --diff-filter=ACMR -- '*.cpp' '*.h'
            ;;
        diff)
            git -C "$REPO_DIR" diff --name-only --diff-filter=ACMR "$BASE_REF"..HEAD -- '*.cpp' '*.h'
            ;;
    esac
}

collect_new() {
    case "$MODE" in
        staged)
            git -C "$REPO_DIR" diff --cached --name-only --diff-filter=A -- '*.cpp' '*.h'
            ;;
        diff)
            git -C "$REPO_DIR" diff --name-only --diff-filter=A "$BASE_REF"..HEAD -- '*.cpp' '*.h'
            ;;
        all)
            :
            ;;
    esac
}

# --- S6: frozen files -------------------------------------------------------
check_frozen_file() {
    local path="$1" baseline="$2" label="$3" actual
    if [ ! -f "$path" ]; then
        echo "FAIL: S6 frozen: missing $label"
        return 1
    fi
    actual="$(wc -l < "$path" | tr -d ' ')"
    if [ "$actual" -gt "$baseline" ]; then
        echo "FAIL: S6 frozen: $label grew to $actual lines (baseline $baseline)"
        return 1
    fi
    echo "PASS: S6 frozen: $label ($actual <= $baseline lines)"
    return 0
}

# --- C3: unowned TODO/FIXME/HACK/XXX markers --------------------------------
check_markers() {
    local rc=0 f disp out line
    for f in "$@"; do
        [ -n "$f" ] || continue
        disp="$(display_path "$f")"
        out="$(awk -v disp="$disp" '
            {
                s = $0
                while (match(s, /(TODO|FIXME|HACK|XXX)[[:space:]]*\(#[0-9]+\)/)) {
                    s = substr(s, 1, RSTART - 1) substr(s, RSTART + RLENGTH)
                }
                if (s ~ /(^|[^A-Za-z0-9_])(TODO|FIXME|HACK|XXX)([^A-Za-z0-9_]|$)/) {
                    print disp ":" FNR ": " $0
                }
            }' "$f")"
        if [ -n "$out" ]; then
            while IFS= read -r line; do
                echo "FAIL: C3 unowned marker: $line"
            done <<< "$out"
            rc=1
        else
            echo "PASS: C3 unowned marker: $disp"
        fi
    done
    return "$rc"
}

# --- U3: SPDX header on newly added files -----------------------------------
check_spdx() {
    local rc=0 f disp
    for f in "$@"; do
        [ -n "$f" ] || continue
        disp="$(display_path "$f")"
        if grep -q 'SPDX-License-Identifier:[[:space:]]*GPL-3.0-or-later' "$f" 2>/dev/null; then
            echo "PASS: U3 SPDX: $disp"
        else
            echo "FAIL: U3 SPDX: $disp missing 'SPDX-License-Identifier: GPL-3.0-or-later'"
            rc=1
        fi
    done
    return "$rc"
}

# --- E1: assert-as-validation in changed PdfTool files ----------------------
check_asserts() {
    local rc=0 f disp hits line
    for f in "$@"; do
        [ -n "$f" ] || continue
        disp="$(display_path "$f")"
        hits="$(grep -nE '(^|[^A-Za-z0-9_])(Q_ASSERT|assert)[[:space:]]*\(' "$f" || true)"
        if [ -n "$hits" ]; then
            while IFS= read -r line; do
                echo "FAIL: E1 assert-as-validation: $disp:$line"
            done <<< "$hits"
            rc=1
        else
            echo "PASS: E1 assert-as-validation: $disp"
        fi
    done
    return "$rc"
}

# --- U2: vendored upstream edits (advisory) ---------------------------------
check_vendored() {
    local f
    for f in "$@"; do
        [ -n "$f" ] || continue
        echo "WARN: U2 vendored-upstream: $(display_path "$f") changed outside a sync(upstream): commit (cherry-pick hygiene)"
    done
    return 0
}

# --- S7: raw new/delete (advisory) ------------------------------------------
check_raw_newdelete() {
    local f disp hits line n
    for f in "$@"; do
        [ -n "$f" ] || continue
        disp="$(display_path "$f")"
        # Strip line comments and block-comment continuations first: the words
        # "new"/"delete" appear constantly in prose, and a raw allocation is a
        # code construct. FNR keeps the reported line numbers honest.
        hits="$(awk '
            {
                line = $0
                gsub(/"[^"]*"/, "\"\"", line)
                sub(/\/\/.*/, "", line)
                if (line ~ /^[[:space:]]*\*/) next
                if (line ~ /(^|[^A-Za-z0-9_])new[[:space:]]+[A-Za-z_<(]/ ||
                    line ~ /(^|[^A-Za-z0-9_])delete([[:space:]]+[A-Za-z_*]|\[\])/) {
                    if (line !~ /operator[[:space:]]+new/) print FNR ": " line
                }
            }' "$f")"
        if [ -n "$hits" ]; then
            n="$(printf '%s\n' "$hits" | grep -c . || true)"
            echo "WARN: S7 raw new/delete: $disp ($n occurrence(s), advisory)"
            while IFS= read -r line; do
                echo "WARN: S7 raw new/delete: $disp:$line"
            done <<< "$(printf '%s\n' "$hits" | head -10)"
        fi
    done
    return 0
}

run_check() {
    local out n
    out="$("$@" 2>&1)"
    if [ -n "$out" ]; then
        printf '%s\n' "$out"
    fi
    n="$(printf '%s\n' "$out" | grep -c '^FAIL' || true)"
    TOTAL_FAIL=$((TOTAL_FAIL + n))
    n="$(printf '%s\n' "$out" | grep -c '^WARN' || true)"
    TOTAL_WARN=$((TOTAL_WARN + n))
}

run_gate() {
    echo "== albdf anti-slop gate (docs/coding-standard.md §11) =="
    case "$MODE" in
        all)    echo "scope: --all (full repo, advisory — the tree has grandfathered violations)" ;;
        staged) echo "scope: staged files" ;;
        diff)   echo "scope: diff vs $BASE_REF" ;;
    esac
    echo

    # S6 — always checked (cheap, absolute): the files may shrink, not grow.
    run_check check_frozen_file "$REPO_DIR/src/PdfTool/pdftoolabstractapplication.h" \
        "$FROZEN_PDFTOOLAPPLICATION_H" "src/PdfTool/pdftoolabstractapplication.h"
    run_check check_frozen_file "$REPO_DIR/src/PdfTool/pdftoolabstractapplication.cpp" \
        "$FROZEN_PDFTOOLAPPLICATION_CPP" "src/PdfTool/pdftoolabstractapplication.cpp"

    local f changed
    local -a authored=() pftool=() new_files=() vendored=()
    changed="$(collect_changed | LC_ALL=C sort || true)"
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        if is_vendored "$f"; then
            vendored+=("$f")
        else
            authored+=("$f")
            case "$f" in src/PdfTool/*) pftool+=("$f") ;; esac
        fi
    done <<< "$changed"

    while IFS= read -r f; do
        [ -n "$f" ] || continue
        new_files+=("$f")
    done <<< "$(collect_new | LC_ALL=C sort || true)"

    local -a authored_abs=() pftool_abs=() new_abs=() vendored_abs=()
    for f in ${authored[@]+"${authored[@]}"}; do authored_abs+=("$REPO_DIR/$f"); done
    for f in ${pftool[@]+"${pftool[@]}"}; do pftool_abs+=("$REPO_DIR/$f"); done
    for f in ${new_files[@]+"${new_files[@]}"}; do new_abs+=("$REPO_DIR/$f"); done
    if [ "$MODE" != "all" ]; then
        for f in ${vendored[@]+"${vendored[@]}"}; do vendored_abs+=("$REPO_DIR/$f"); done
    fi

    if [ "${#authored_abs[@]}" -gt 0 ]; then
        run_check check_markers "${authored_abs[@]}"
        run_check check_raw_newdelete "${authored_abs[@]}"
    else
        echo "INFO: no changed authored files in scope (C3, S7 skipped)"
    fi

    if [ "${#new_abs[@]}" -gt 0 ]; then
        run_check check_spdx "${new_abs[@]}"
    else
        echo "INFO: no newly added authored files in scope (U3 skipped)"
    fi

    if [ "${#pftool_abs[@]}" -gt 0 ]; then
        run_check check_asserts "${pftool_abs[@]}"
    else
        echo "INFO: no changed src/PdfTool files in scope (E1 skipped)"
    fi

    if [ "${#vendored_abs[@]}" -gt 0 ]; then
        run_check check_vendored "${vendored_abs[@]}"
    fi

    echo
    echo "== summary =="
    echo "FAIL: $TOTAL_FAIL   WARN: $TOTAL_WARN"
    if [ "$TOTAL_FAIL" -ne 0 ]; then
        echo "SLOP GATE: FAIL"
        return 1
    fi
    echo "SLOP GATE: PASS"
    return 0
}

# --- self-test --------------------------------------------------------------
run_self_test() {
    local tmp st_fail=0
    tmp="$(mktemp -d "${TMPDIR:-/tmp}/albdf-slop-selftest.XXXXXX")"

    printf 'a\nb\nc\n' > "$tmp/frozen_ok.cpp"
    printf 'a\nb\nc\nd\n' > "$tmp/frozen_bad.cpp"
    printf '// SPDX-License-Identifier: GPL-3.0-or-later\nint ok;\n' > "$tmp/spdx_ok.cpp"
    printf 'int missing_header;\n' > "$tmp/spdx_bad.cpp"
    printf '// TODO(#123): owned marker\nint ok;\n' > "$tmp/marker_ok.cpp"
    printf '// FIXME: unowned marker\nint bad;\n' > "$tmp/marker_bad.cpp"
    printf '// TODOs are not markers; XXXY and HACKERS are not either\nint ok;\n' > "$tmp/marker_word.cpp"
    printf '// TODO(#1) then a bare FIXME on the same line\n' > "$tmp/marker_mixed.cpp"
    printf 'void f(int x) { Q_ASSERT(x); }\n' > "$tmp/assert_bad.cpp"
    printf 'static_assert(sizeof(int) > 0, "x");\nint assertion_count;\n' > "$tmp/assert_ok.cpp"
    printf 'int* p = new int;\ndelete p;\n' > "$tmp/newdelete_bad.cpp"
    printf 'obj->deleteLater();\nvoid f() = delete;\n' > "$tmp/newdelete_ok.cpp"

    assert_trigger() {
        local desc="$1"; shift
        local out
        out="$("$@" 2>&1)"
        if printf '%s' "$out" | grep -q '^FAIL'; then
            echo "SELFTEST ok: $desc (triggers)"
        else
            echo "SELFTEST FAIL: expected a FAIL for: $desc"
            printf '%s\n' "$out" | sed 's/^/    /'
            st_fail=$((st_fail + 1))
        fi
    }
    assert_clean() {
        local desc="$1"; shift
        local out rc
        out="$("$@" 2>&1)"; rc=$?
        if [ "$rc" -ne 0 ] || printf '%s' "$out" | grep -q '^FAIL'; then
            echo "SELFTEST FAIL: expected clean for: $desc"
            printf '%s\n' "$out" | sed 's/^/    /'
            st_fail=$((st_fail + 1))
        else
            echo "SELFTEST ok: $desc (clean)"
        fi
    }
    assert_warn() {
        local desc="$1"; shift
        local out
        out="$("$@" 2>&1)"
        if printf '%s' "$out" | grep -q '^WARN'; then
            echo "SELFTEST ok: $desc (warns)"
        else
            echo "SELFTEST FAIL: expected a WARN for: $desc"
            printf '%s\n' "$out" | sed 's/^/    /'
            st_fail=$((st_fail + 1))
        fi
    }

    assert_clean   "S6 frozen at baseline"      check_frozen_file "$tmp/frozen_ok.cpp" 3 "frozen_ok"
    assert_trigger "S6 frozen over baseline"    check_frozen_file "$tmp/frozen_bad.cpp" 3 "frozen_bad"
    assert_clean   "U3 SPDX present"            check_spdx "$tmp/spdx_ok.cpp"
    assert_trigger "U3 SPDX missing"            check_spdx "$tmp/spdx_bad.cpp"
    assert_clean   "C3 owned marker"            check_markers "$tmp/marker_ok.cpp"
    assert_trigger "C3 bare marker"             check_markers "$tmp/marker_bad.cpp"
    assert_clean   "C3 marker-like words"       check_markers "$tmp/marker_word.cpp"
    assert_trigger "C3 owned+bare on one line"  check_markers "$tmp/marker_mixed.cpp"
    assert_trigger "E1 assert-as-validation"    check_asserts "$tmp/assert_bad.cpp"
    assert_clean   "E1 static_assert/assertion" check_asserts "$tmp/assert_ok.cpp"
    assert_warn    "S7 raw new/delete"          check_raw_newdelete "$tmp/newdelete_bad.cpp"
    assert_clean   "S7 deleteLater/= delete"    check_raw_newdelete "$tmp/newdelete_ok.cpp"
    assert_warn    "U2 vendored edit"           check_vendored "$tmp/whatever.cpp"

    rm -rf "$tmp"
    echo
    if [ "$st_fail" -ne 0 ]; then
        echo "SELF-TEST: FAIL ($st_fail)"
        return 1
    fi
    echo "SELF-TEST: PASS"
    return 0
}

# --- main -------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --all)       MODE="all"; shift ;;
        --staged)    MODE="staged"; shift ;;
        --base)      BASE_REF="${2:?--base requires a ref}"; shift 2 ;;
        --self-test) SELF_TEST=1; shift ;;
        -h|--help)   usage; exit 0 ;;
        *) echo "check-slop: unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [ "$SELF_TEST" -eq 1 ]; then
    run_self_test
    exit $?
fi

if [ "$MODE" = "diff" ]; then
    if [ -z "$BASE_REF" ]; then
        BASE_REF="$(resolve_base)"
    fi
    if [ -z "$BASE_REF" ] || ! git -C "$REPO_DIR" rev-parse --verify --quiet "$BASE_REF" >/dev/null 2>&1; then
        echo "check-slop: ERROR: cannot resolve base ref (origin/main or $FORK_BASE); use --all or --base" >&2
        exit 2
    fi
fi

run_gate
exit $?
