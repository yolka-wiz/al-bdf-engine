# albdf — headless PDF editing library + CLI (fork of PDF4QT, MIT)
# v1 scope: Pdf4QtLibCore + albdf + UnitTests. NO GUI by default (ADR-0002);
# optional GUI layer (vendored PDF4QT apps) behind ALBDF_BUILD_GUI=ON (M12).
cmake_minimum_required(VERSION 3.16)

set(ALBDF_VERSION 0.4.0)
set(PDF4QT_VERSION 1.6.0.0)   # kept for upstream compatibility in vendored CMake
project(albdf VERSION ${ALBDF_VERSION} LANGUAGES CXX)

add_compile_definitions(PDF4QT_PROJECT_VERSION="${ALBDF_VERSION}")
add_compile_definitions(QT_NO_EMIT)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ALBDF_BUILD_TESTS "Build UnitTests" ON)
option(ALBDF_INSTALL_INCLUDE "Install include headers" ON)
# OPTIONAL GUI layer (M12): builds the 5 vendored PDF4QT GUI dirs
# (Pdf4QtLibWidgets, Pdf4QtLibGui, Pdf4QtEditor, Pdf4QtViewer, Pdf4QtPageMaster).
# OFF by default — the headless contract (ADR-0002) stands: core + CLI + tests
# never depend on GUI code and must stay deterministic and display-free.
option(ALBDF_BUILD_GUI "Build the optional vendored PDF4QT GUI libs + apps" OFF)

# vcpkg toolchain (deps: tbb openssl lcms zlib openjpeg freetype libjpeg-turbo
# libpng blend2d — see vcpkg.json; blend2d MUST come from vcpkg, see baseline doc)
if(DEFINED ENV{VCPKG_ROOT} AND NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
endif()

# Qt components: core + CLI only need Core/Gui/Xml/Svg/Test (headless).
# Widgets/PrintSupport/Concurrent are required by the OPTIONAL GUI layer
# (ALBDF_BUILD_GUI=ON); they ship in qt6-base-dev, so the headless default is
# unaffected. Do NOT add TextToSpeech (patched out — fork divergence).
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Xml Svg Test Widgets PrintSupport Concurrent)

# Needed by the vendored GUI dirs so their self-includes resolve to the source
# dir (upstream gets this via qt_standard_project_setup, which we don't use).
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Deps required by the vendored core (upstream list, permissive only — ADR-0004)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Freetype REQUIRED)
find_package(OpenJPEG CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(blend2d CONFIG REQUIRED)

# RTL pipeline deps (M4): HarfBuzz (MIT) + FriBidi (LGPL-2.1+) via vcpkg.
# harfbuzz ships a CMake config; FriBidi only ships pkg-config.
find_package(harfbuzz CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FRIBIDI REQUIRED IMPORTED_TARGET fribidi)

# fontconfig: pdffont.cpp uses it for system-font substitution on all Unix
# (incl. macOS — Q_OS_UNIX). On Linux QtGui pulls it in transitively, but the
# core must link it explicitly so macOS (where Qt uses CoreText, no
# fontconfig) and non-Qt consumers resolve Fc* symbols. brew provides it.
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)

if(VCPKG_TOOLCHAIN)
    find_package(lcms2 REQUIRED)
    set(LCMS2_LIBRARIES lcms2::lcms2 CACHE INTERNAL "LCMS2 libraries")
else()
    find_library(LCMS2_LIBRARIES lcms2 REQUIRED)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTORCC_OPTIONS "--threshold;0;--compress;9")

if (UNIX AND NOT APPLE AND CMAKE_COMPILER_IS_GNUCXX)
    set(LINUX_GCC ON)
endif()
if (UNIX AND NOT APPLE)
    set(PDF4QT_LINUX ON)
endif()
if (LINUX_GCC)
    find_package(TBB REQUIRED)
endif()

# Install dirs (kept identical to upstream so vendored CMake works unchanged)
include(GNUInstallDirs)
if(PDF4QT_LINUX)
    set(PDF4QT_INSTALL_LIB_DIR_IMPL ${CMAKE_INSTALL_LIBDIR})
    set(PDF4QT_INSTALL_BIN_DIR_IMPL ${CMAKE_INSTALL_BINDIR})
else()
    set(PDF4QT_INSTALL_LIB_DIR_IMPL ${CMAKE_INSTALL_BINDIR})
    set(PDF4QT_INSTALL_BIN_DIR_IMPL ${CMAKE_INSTALL_BINDIR})
endif()
set(PDF4QT_INSTALL_LIB_DIR ${PDF4QT_INSTALL_LIB_DIR_IMPL})
set(PDF4QT_INSTALL_BIN_DIR ${PDF4QT_INSTALL_BIN_DIR_IMPL})
set(PDF4QT_INSTALL_SHARE_DIR ${CMAKE_INSTALL_DATAROOTDIR})
set(PDF4QT_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR})
# NOTE: INSTALL_INCLUDEDIR intentionally left UNSET — upstream leaves it empty,
# so config.h + export headers land at ${CMAKE_BINARY_DIR} root (matches baseline).
if(PDF4QT_LINUX)
    set(PDF4QT_PLUGINS_DIR ${PDF4QT_INSTALL_LIB_DIR}/pdf4qt)
    set(PDF4QT_INSTALL_TRANS_DIR ${PDF4QT_INSTALL_SHARE_DIR}/pdf4qt/translations)
else()
    set(PDF4QT_PLUGINS_DIR ${PDF4QT_INSTALL_LIB_DIR}/pdfplugins)
    set(PDF4QT_INSTALL_TRANS_DIR ${PDF4QT_INSTALL_BIN_DIR}/translations)
endif()
cmake_path(RELATIVE_PATH PDF4QT_PLUGINS_DIR BASE_DIRECTORY "${PDF4QT_INSTALL_BIN_DIR}" OUTPUT_VARIABLE PDF4QT_PLUGINS_RELATIVE_PATH)
cmake_path(RELATIVE_PATH PDF4QT_INSTALL_TRANS_DIR BASE_DIRECTORY "${PDF4QT_INSTALL_BIN_DIR}" OUTPUT_VARIABLE PDF4QT_TRANSLATIONS_RELATIVE_PATH)
configure_file(config.h.cmake config.h)

add_subdirectory(Pdf4QtLibCore)
add_subdirectory(PdfTool)   # dir keeps upstream name; target produces the albdf binary

if(ALBDF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(UnitTests)
endif()

if(ALBDF_BUILD_GUI)
    # Vendored PDF4QT GUI layer (upstream v1.6.0.0, see ADR-0002 addendum).
    # Order matters: LibWidgets before LibGui (LibGui links it by name);
    # the 3 apps sit on top. Pdf4QtEditorPlugins is intentionally NOT wired
    # (runtime-loaded, degrades gracefully); Pdf4QtLibGui/main.cpp is not in
    # upstream's source list and stays out.
    add_subdirectory(Pdf4QtLibWidgets)
    add_subdirectory(Pdf4QtLibGui)
    add_subdirectory(Pdf4QtEditor)
    add_subdirectory(Pdf4QtViewer)
    add_subdirectory(Pdf4QtPageMaster)
endif()

# ---------------------------------------------------------------------------
# W7 — release packaging (DB #26). The albdf binary and Pdf4QtLibCore shared
# library get their own install() rules in their subdirectory CMakeLists; this
# block adds what a staged release package needs on top of those:
#   - a relocatable $ORIGIN RPATH for the installed binary. CMake bakes the
#     build-tree path into RUNPATH for libraries linked from the build tree,
#     which would leak a machine-specific path into the release tarball
#     (scripts/package.sh verifies this with readelf).
#   - the public headers (Pdf4QtLibCore/sources/*.h) plus the generated
#     pdf4qtlibcore_export.h they include, under include/Pdf4QtLibCore.
#   - the man page and the license file.
# scripts/package.sh drives this via `cmake --install <build> --prefix <stage>`.
# ---------------------------------------------------------------------------
if(UNIX AND NOT APPLE)
    set_target_properties(albdf PROPERTIES
        INSTALL_RPATH "\$ORIGIN/../${PDF4QT_INSTALL_LIB_DIR}")
elseif(APPLE)
    # macOS dylibs resolve relative to the binary via @loader_path (the
    # equivalent of $ORIGIN). The staged tree is bin/albdf + lib/*.dylib.
    set_target_properties(albdf PROPERTIES
        INSTALL_RPATH "@loader_path/../${PDF4QT_INSTALL_LIB_DIR}")
endif()

if(ALBDF_INSTALL_INCLUDE)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Pdf4QtLibCore/sources/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Pdf4QtLibCore
            FILES_MATCHING PATTERN "*.h")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pdf4qtlibcore_export.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Pdf4QtLibCore)
endif()

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../docs/albdf.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/albdf)
