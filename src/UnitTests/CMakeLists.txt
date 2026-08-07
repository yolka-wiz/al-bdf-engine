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

add_executable(UnitTests
	tst_lexicalanalyzertest.cpp
)

target_link_libraries(UnitTests PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(UnitTests PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTests  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTests")

add_executable(UnitTestsImageOptimizer
	tst_imageoptimizertest.cpp
)

target_link_libraries(UnitTestsImageOptimizer PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(UnitTestsImageOptimizer PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsImageOptimizer  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsImageOptimizer")

add_executable(UnitTestsFontEncoding
	tst_fontencodingtest.cpp
)

target_link_libraries(UnitTestsFontEncoding PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(UnitTestsFontEncoding PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsFontEncoding  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsFontEncoding")

configure_file(../../docs/research/baseline-out/test-baseline.pdf ${CMAKE_CURRENT_BINARY_DIR}/test-baseline.pdf COPYONLY)

add_executable(UnitTestsRecognizeText
	tst_recognizetext.cpp
)

target_link_libraries(UnitTestsRecognizeText PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

target_compile_definitions(UnitTestsRecognizeText PRIVATE TEST_BASELINE_PDF="${CMAKE_CURRENT_BINARY_DIR}/test-baseline.pdf")

set_target_properties(UnitTestsRecognizeText PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsRecognizeText  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsRecognizeText")

# delete-object integration test: runs albdf against the baseline fixture.
add_executable(UnitTestsDeleteObject
	tst_deleteobjecttest.cpp
)

target_link_libraries(UnitTestsDeleteObject PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

target_compile_definitions(UnitTestsDeleteObject PRIVATE TEST_BASELINE_PDF="${CMAKE_CURRENT_BINARY_DIR}/test-baseline.pdf")

set_target_properties(UnitTestsDeleteObject PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsDeleteObject  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsDeleteObject")

add_dependencies(UnitTestsDeleteObject albdf)

# add-text integration test: runs albdf against the baseline fixture.
add_executable(UnitTestsAddText
	tst_addtexttest.cpp
)

target_link_libraries(UnitTestsAddText PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

target_compile_definitions(UnitTestsAddText PRIVATE TEST_BASELINE_PDF="${CMAKE_CURRENT_BINARY_DIR}/test-baseline.pdf")

set_target_properties(UnitTestsAddText PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsAddText  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsAddText")

add_dependencies(UnitTestsAddText albdf)

# RTL add-text integration test: runs albdf with --rtl against the blank
# fixture and the bundled OFL fonts (Vazirmatn / Noto Naskh Arabic / Noto Sans
# Hebrew). The fonts are MIT/OFL permissive; see src/tests/fonts/README.
add_executable(UnitTestsRtlAddText
	tst_rtladdtexttest.cpp
)

target_link_libraries(UnitTestsRtlAddText PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

target_compile_definitions(UnitTestsRtlAddText PRIVATE
    TEST_BLANK_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/blank.pdf"
    TEST_FONT_PERSIAN="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/Vazirmatn-Regular.ttf"
    TEST_FONT_ARABIC="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/NotoNaskhArabic-Regular.ttf"
    TEST_FONT_HEBREW="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/NotoSansHebrew-Regular.ttf"
)

set_target_properties(UnitTestsRtlAddText PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsRtlAddText  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsRtlAddText")

add_dependencies(UnitTestsRtlAddText albdf)

# /ActualText marked-content regression test (R#4): RTL add-text (lam-alef),
# a second LTR add-text on the same page (content-editor stream rewrite),
# then fetch-text must still yield the full ligature.
add_executable(UnitTestsActualText
	tst_actualtexttest.cpp
)

target_link_libraries(UnitTestsActualText PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

target_compile_definitions(UnitTestsActualText PRIVATE
    TEST_BLANK_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/blank.pdf"
    TEST_FONT_PERSIAN="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/Vazirmatn-Regular.ttf"
)

set_target_properties(UnitTestsActualText PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(NAME UnitTestsActualText COMMAND "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsActualText")
set_tests_properties(UnitTestsActualText PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_dependencies(UnitTestsActualText albdf)

# RTL-aware search integration test (M5): drives albdf add-text +
# search-text on RTL corpus (Hebrew, Persian digits, ZWNJ, tashkeel,
# mixed LTR+RTL) and asserts match counts.
add_executable(UnitTestsSearchText
	tst_searchtexttest.cpp
)

target_link_libraries(UnitTestsSearchText PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

target_compile_definitions(UnitTestsSearchText PRIVATE
    TEST_BLANK_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/blank.pdf"
    TEST_FONT_PERSIAN="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/Vazirmatn-Regular.ttf"
    TEST_FONT_ARABIC="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/NotoNaskhArabic-Regular.ttf"
    TEST_FONT_HEBREW="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/NotoSansHebrew-Regular.ttf"
)

set_target_properties(UnitTestsSearchText PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsSearchText  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsSearchText")

add_dependencies(UnitTestsSearchText albdf)

# Golden-image regression harness: renders src/tests/fixtures/*.pdf with
# albdf render (72 dpi, PNG) and compares SHA-256 against the committed
# goldens in src/tests/golden/. See tst_goldentest.cpp for details.
add_executable(UnitTestsGolden
	tst_goldentest.cpp
)

target_link_libraries(UnitTestsGolden PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(UnitTestsGolden PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)

target_compile_definitions(UnitTestsGolden PRIVATE
    ALBDF_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../tests"
)

add_dependencies(UnitTestsGolden albdf)

add_test(NAME UnitTestsGolden COMMAND "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsGolden")
set_tests_properties(UnitTestsGolden PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Forms + signatures integration test: runs form-list/form-fill/sign/verify
# against a synthetic AcroForm fixture and a generated PKCS#12 certificate.
add_executable(UnitTestsFormSignature
	tst_formsignaturetest.cpp
)

target_link_libraries(UnitTestsFormSignature PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(UnitTestsFormSignature PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(NAME UnitTestsFormSignature COMMAND "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsFormSignature")
set_tests_properties(UnitTestsFormSignature PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_dependencies(UnitTestsFormSignature albdf)

# Page-ops integration test: runs albdf rotate / move-page / delete-page
# against the deterministic multipage fixture (5 pages, distinct text per
# page) and asserts geometry changes, content relocation, page-count
# reduction, byte-deterministic output and clean round-trip reopening.
add_executable(UnitTestsPageOps
	tst_pageopstest.cpp
)

target_link_libraries(UnitTestsPageOps PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

target_compile_definitions(UnitTestsPageOps PRIVATE TEST_MULTIPAGE_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/multipage.pdf")

set_target_properties(UnitTestsPageOps PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(NAME UnitTestsPageOps COMMAND "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsPageOps")
set_tests_properties(UnitTestsPageOps PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_dependencies(UnitTestsPageOps albdf)

# Redaction integration test: runs albdf redact against the annotated
# redact-annotated.pdf fixture (two /Subtype /Redact annotations) and asserts
# pixel-level redaction (solid black bars over the annotated regions, public
# content preserved), page-size preservation, info round-trip, byte-determinism
# and the invalid-arguments exit-code contract.
add_executable(UnitTestsRedact
	tst_redacttest.cpp
)

target_link_libraries(UnitTestsRedact PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

target_compile_definitions(UnitTestsRedact PRIVATE TEST_REDACT_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/redact-annotated.pdf")

set_target_properties(UnitTestsRedact PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
# render CLI regression suite (M10, DB #32 F#1 / DB #33 F#2): fuzz-found
# abusive render arguments (--page-first 0, --page-last 999999999,
# --image-res-dpi 999999) must be rejected with the documented error code
# (7) instead of crashing with SIGABRT or hanging; valid renders must
# stay byte-deterministic.
add_executable(UnitTestsRender
	tst_rendertest.cpp
)

target_link_libraries(UnitTestsRender PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

target_compile_definitions(UnitTestsRender PRIVATE TEST_MULTIPAGE_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/multipage.pdf")

set_target_properties(UnitTestsRender PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(NAME UnitTestsRedact COMMAND "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsRedact")
set_tests_properties(UnitTestsRedact PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_dependencies(UnitTestsRedact albdf)

add_test(NAME UnitTestsRender COMMAND "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsRender")
set_tests_properties(UnitTestsRender PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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
