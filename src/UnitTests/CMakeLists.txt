# MIT License
#
# Copyright (c) 2018-2025 Jakub Melka and Contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# add_albdf_test(<target> <source> [HEADLESS] [DEFINITIONS <define>...])
#
# Registers one Qt Test suite: creates the executable, links the standard test
# libraries, points its output at the install bin/lib dirs, wires the ctest
# entry, and (with HEADLESS) exports QT_QPA_PLATFORM=offscreen into the test
# environment. DEFINITIONS forwards fixture/font macros such as TEST_*.
# Suites that shell out to the CLI must still add
# `add_dependencies(<target> albdf)` themselves.
function(add_albdf_test name source)
    cmake_parse_arguments(ARG "HEADLESS" "" "DEFINITIONS" ${ARGN})

    add_executable(${name} ${source})

    target_link_libraries(${name} PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

    set_target_properties(${name} PROPERTIES
        WIN32_EXECUTABLE OFF
        MACOSX_BUNDLE OFF
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
    )

    if(ARG_DEFINITIONS)
        target_compile_definitions(${name} PRIVATE ${ARG_DEFINITIONS})
    endif()

    add_test(NAME ${name} COMMAND "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/${name}")

    if(ARG_HEADLESS)
        set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endfunction()

add_albdf_test(UnitTests tst_lexicalanalyzertest.cpp)

# RTL normalizer unit test: PDFRTLTextNormalizer::invertToLogical
# (visual->logical re-inversion for the GUI clipboard copy path).
add_albdf_test(UnitTestsRtlNormalizer tst_rtlnormalizertest.cpp HEADLESS)

add_albdf_test(UnitTestsImageOptimizer tst_imageoptimizertest.cpp)

add_albdf_test(UnitTestsFontEncoding tst_fontencodingtest.cpp)

configure_file(../../docs/research/baseline-out/test-baseline.pdf ${CMAKE_CURRENT_BINARY_DIR}/test-baseline.pdf COPYONLY)

add_albdf_test(UnitTestsRecognizeText tst_recognizetext.cpp
    DEFINITIONS TEST_BASELINE_PDF="${CMAKE_CURRENT_BINARY_DIR}/test-baseline.pdf")

# delete-object integration test: runs albdf against the baseline fixture.
add_albdf_test(UnitTestsDeleteObject tst_deleteobjecttest.cpp
    DEFINITIONS TEST_BASELINE_PDF="${CMAKE_CURRENT_BINARY_DIR}/test-baseline.pdf")
add_dependencies(UnitTestsDeleteObject albdf)

# add-text integration test: runs albdf against the baseline fixture.
add_albdf_test(UnitTestsAddText tst_addtexttest.cpp
    DEFINITIONS TEST_BASELINE_PDF="${CMAKE_CURRENT_BINARY_DIR}/test-baseline.pdf")
add_dependencies(UnitTestsAddText albdf)

# RTL add-text integration test: runs albdf with --rtl against the blank
# fixture and the bundled OFL fonts (Vazirmatn / Noto Naskh Arabic / Noto Sans
# Hebrew). The fonts are MIT/OFL permissive; see src/tests/fonts/README.
add_albdf_test(UnitTestsRtlAddText tst_rtladdtexttest.cpp
    DEFINITIONS
        TEST_BLANK_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/blank.pdf"
        TEST_FONT_PERSIAN="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/Vazirmatn-Regular.ttf"
        TEST_FONT_ARABIC="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/NotoNaskhArabic-Regular.ttf"
        TEST_FONT_HEBREW="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/NotoSansHebrew-Regular.ttf")
add_dependencies(UnitTestsRtlAddText albdf)

# /ActualText marked-content regression test (R#4): RTL add-text (lam-alef),
# a second LTR add-text on the same page (content-editor stream rewrite),
# then fetch-text must still yield the full ligature.
add_albdf_test(UnitTestsActualText tst_actualtexttest.cpp HEADLESS
    DEFINITIONS
        TEST_BLANK_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/blank.pdf"
        TEST_FONT_PERSIAN="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/Vazirmatn-Regular.ttf")
add_dependencies(UnitTestsActualText albdf)

# R3.1 shared write-back helper: drives PDFPageContentRewriter directly against
# the indirect-/Resources + indirect-/Contents fixture (Append stream -> array,
# array growth, Replace, resource preservation, byte-determinism) and through
# the RTL CLI path that uses Append in production.
add_albdf_test(UnitTestsPageContentRewriter tst_pagecontentrewritertest.cpp HEADLESS
    DEFINITIONS
        TEST_IMAGE_DOC_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/image-doc.pdf"
        TEST_FONT_PERSIAN="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/Vazirmatn-Regular.ttf")
add_dependencies(UnitTestsPageContentRewriter albdf)

# M14: FreeText annotation with RTL contents must embed a shaped Type0
# font (FontFile2) in its appearance stream — not base-14 Helvetica (tofu).
add_albdf_test(UnitTestsRtlFreeText tst_rtlfreetexttest.cpp HEADLESS
    DEFINITIONS
        TEST_BLANK_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/blank.pdf"
        TEST_FONT_ARABIC="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/NotoNaskhArabic-Regular.ttf")
add_dependencies(UnitTestsRtlFreeText albdf)

# RTL-aware search integration test (M5): drives albdf add-text +
# search-text on RTL corpus (Hebrew, Persian digits, ZWNJ, tashkeel,
# mixed LTR+RTL) and asserts match counts.
add_albdf_test(UnitTestsSearchText tst_searchtexttest.cpp
    DEFINITIONS
        TEST_BLANK_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/blank.pdf"
        TEST_FONT_PERSIAN="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/Vazirmatn-Regular.ttf"
        TEST_FONT_ARABIC="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/NotoNaskhArabic-Regular.ttf"
        TEST_FONT_HEBREW="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/NotoSansHebrew-Regular.ttf")
add_dependencies(UnitTestsSearchText albdf)

# Golden-image regression harness: renders src/tests/fixtures/*.pdf with
# albdf render (72 dpi, PNG) and compares SHA-256 against the committed
# goldens in src/tests/golden/. See tst_goldentest.cpp for details.
add_albdf_test(UnitTestsGolden tst_goldentest.cpp HEADLESS
    DEFINITIONS ALBDF_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../tests")
add_dependencies(UnitTestsGolden albdf)

# Forms + signatures integration test: runs form-list/form-fill/sign/verify
# against a synthetic AcroForm fixture and a generated PKCS#12 certificate.
add_albdf_test(UnitTestsFormSignature tst_formsignaturetest.cpp HEADLESS
    DEFINITIONS TEST_FONT_ARABIC="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/NotoNaskhArabic-Regular.ttf")
add_dependencies(UnitTestsFormSignature albdf)

# Page-ops integration test: runs albdf rotate / move-page / delete-page
# against the deterministic multipage fixture (5 pages, distinct text per
# page) and asserts geometry changes, content relocation, page-count
# reduction, byte-deterministic output and clean round-trip reopening.
add_albdf_test(UnitTestsPageOps tst_pageopstest.cpp HEADLESS
    DEFINITIONS TEST_MULTIPAGE_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/multipage.pdf")
add_dependencies(UnitTestsPageOps albdf)

# Redaction integration test: runs albdf redact against the annotated
# redact-annotated.pdf fixture (two /Subtype /Redact annotations) and asserts
# pixel-level redaction (solid black bars over the annotated regions, public
# content preserved), page-size preservation, info round-trip, byte-determinism
# and the invalid-arguments exit-code contract.
add_albdf_test(UnitTestsRedact tst_redacttest.cpp HEADLESS
    DEFINITIONS TEST_REDACT_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/redact-annotated.pdf")
add_dependencies(UnitTestsRedact albdf)

# render CLI regression suite (M10, DB #32 F#1 / DB #33 F#2): fuzz-found
# abusive render arguments (--page-first 0, --page-last 999999999,
# --image-res-dpi 999999) must be rejected with the documented error code
# (7) instead of crashing with SIGABRT or hanging; valid renders must
# stay byte-deterministic.
add_albdf_test(UnitTestsRender tst_rendertest.cpp HEADLESS
    DEFINITIONS TEST_MULTIPAGE_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/multipage.pdf")
add_dependencies(UnitTestsRender albdf)

# CLI smoke test: exercises fetch-text/render/info/unite on the fixtures
# (and recognize-text/delete-object when present) and asserts invariants.
find_program(BASH_PROGRAM bash)
if(BASH_PROGRAM)
    add_test(NAME SmokeCli
        COMMAND "${BASH_PROGRAM}"
                "${CMAKE_CURRENT_SOURCE_DIR}/../tests/smoke.sh"
                "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/albdf"
                "${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures")
    set_tests_properties(SmokeCli PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
