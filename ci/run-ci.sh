#!/bin/bash
# albdf CI gate — run on a bare container after reinstall-toolchain.sh.
#
#   ci/run-ci.sh [--skip-asan] [--skip-format] [--skip-release] [--only-format]
#
# Stages:
#   1. configure + build (Release, offscreen-capable)
#   2. ctest (offscreen)
#   3. ASAN/UBSAN build + ctest (Debug + sanitizers)
#   4. clang-format gate (authored files only — vendored upstream is exempt)
#
# Stage-selection flags (backward compatible — the bare invocation still
# runs all four stages, as the local bare-container flow expects):
#   --skip-release  skip stages 1-2 (assumes a green Release build already
#                   exists; used by the hosted-CI ASAN job)
#   --skip-asan     skip stage 3
#   --skip-format   skip stage 4
#   --only-format   run only stage 4 (used by the hosted-CI format job)
#
# Exit code: 0 = all green; 1 = any stage failed.
set -u

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="$REPO_DIR/src"
SKIP_ASAN=0
SKIP_FORMAT=0
SKIP_RELEASE=0
ONLY_FORMAT=0
for arg in "$@"; do
    case "$arg" in
        --skip-asan) SKIP_ASAN=1 ;;
        --skip-format) SKIP_FORMAT=1 ;;
        --skip-release) SKIP_RELEASE=1 ;;
        --only-format) ONLY_FORMAT=1 ;;
    esac
done

export VCPKG_ROOT="${VCPKG_ROOT:-/workspace/vcpkg}"
VCPKG_TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
FAILED=0

step() { echo; echo "=== $1 ==="; }

# The list of files WE authored (everything changed since the fork base).
# Vendored upstream files are intentionally NOT formatted (cherry-pick hygiene).
# Upstream-derived files we only MODIFIED (created in the M1 fork import) are
# excluded too — their original formatting is upstream's, and reformatting the
# whole file would destroy cherry-pick diffs. We keep our own added lines styled
# to match the surrounding code instead.
AUTHORED_FILES="$(
    cd "$REPO_DIR" && git diff --name-only 6bf5047..HEAD -- '*.cpp' '*.h' \
        | grep -vE '^src/PdfTool/pdftoolabstractapplication\.(cpp|h)$' \
        | grep -vE '^src/PdfTool/main\.cpp$' \
        | grep -vE '^src/Pdf4QtLibCore/sources/pdfpagecontenteditorprocessor\.(cpp|h)$' \
        | grep -vE '^src/Pdf4QtLibCore/sources/pdfpagecontenteditorcontentstreambuilder\.(cpp|h)$' \
        | grep -vE '^src/Pdf4QtLibCore/sources/pdftextlayout\.cpp$' \
        | grep -vE '^src/Pdf4QtLibCore/sources/pdfdocumentbuilder\.cpp$' \
        | grep -vE '^src/Pdf4QtLibCore/sources/pdfutils\.cpp$' \
        | grep -vE '^src/PdfTool/pdftoolrender\.cpp$' \
        | grep -vE '^src/Pdf4QtLibGui/' \
        | grep -vE '^src/Pdf4QtLibWidgets/' \
        | grep -vE '^src/Pdf4QtEditor/' \
        | grep -vE '^src/Pdf4QtViewer/' \
        | grep -vE '^src/Pdf4QtPageMaster/'
)"

# All build stages run from the source dir (format stage uses absolute
# paths, so it is unaffected by cwd).
cd "$SRC_DIR"

if [ "$ONLY_FORMAT" -eq 0 ]; then
    if [ "$SKIP_RELEASE" -eq 0 ]; then
        step "1/4 configure + build (Release)"
        cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" \
              -DALBDF_BUILD_TESTS=ON > /tmp/ci-build.log 2>&1
        if [ $? -ne 0 ]; then echo "configure FAILED"; tail -20 /tmp/ci-build.log; FAILED=1; fi
        cmake --build build >> /tmp/ci-build.log 2>&1
        if [ $? -ne 0 ]; then echo "build FAILED"; tail -20 /tmp/ci-build.log; FAILED=1; fi

        step "2/4 ctest (offscreen)"
        export QT_QPA_PLATFORM=offscreen
        ctest --test-dir build --output-on-failure > /tmp/ci-ctest.log 2>&1
        if [ $? -ne 0 ]; then echo "ctest FAILED"; tail -20 /tmp/ci-ctest.log; FAILED=1; else
            tail -2 /tmp/ci-ctest.log
        fi
    fi

    if [ "$SKIP_ASAN" -eq 0 ]; then
        step "3/4 ASAN/UBSAN build + ctest"
        cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
              -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" \
              -DALBDF_BUILD_TESTS=ON \
              -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
              -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
              -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
              -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined" \
              -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON > /tmp/ci-asan-cfg.log 2>&1
        if [ $? -ne 0 ]; then echo "asan configure FAILED"; tail -10 /tmp/ci-asan-cfg.log; FAILED=1; fi
        cmake --build build-asan >> /tmp/ci-asan-build.log 2>&1
        if [ $? -ne 0 ]; then echo "asan build FAILED"; tail -10 /tmp/ci-asan-build.log; FAILED=1; fi
        export LD_LIBRARY_PATH="$SRC_DIR/build-asan/lib:${LD_LIBRARY_PATH:-}"
        export ASAN_OPTIONS=detect_leaks=0:abort_on_error=1
        export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
        ctest --test-dir build-asan --output-on-failure > /tmp/ci-asan-ctest.log 2>&1
        if [ $? -ne 0 ]; then echo "asan ctest FAILED"; tail -20 /tmp/ci-asan-ctest.log; FAILED=1; else
            tail -2 /tmp/ci-asan-ctest.log
        fi
        unset LD_LIBRARY_PATH
    fi
fi

if [ "$SKIP_FORMAT" -eq 0 ]; then
    step "4/4 clang-format gate (authored files)"
    UNFORMATTED=0
    for f in $AUTHORED_FILES; do
        if ! clang-format --dry-run --Werror "$REPO_DIR/$f" > /dev/null 2>&1; then
            echo "UNFORMATTED: $f"
            UNFORMATTED=$((UNFORMATTED+1))
        fi
    done
    if [ "$UNFORMATTED" -ne 0 ]; then
        echo "clang-format gate FAILED ($UNFORMATTED files)"
        FAILED=1
    else
        echo "clang-format gate OK ($(echo "$AUTHORED_FILES" | wc -l) files)"
    fi
fi

echo
if [ "$FAILED" -eq 0 ]; then
    echo "CI: ALL GREEN"
else
    echo "CI: FAILED"
fi
exit "$FAILED"
