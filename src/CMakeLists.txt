# pdfedit — headless PDF editing library + CLI (fork of PDF4QT, MIT)
# v1 scope: Pdf4QtLibCore + PdfTool + UnitTests. NO GUI (ADR-0002).
cmake_minimum_required(VERSION 3.16)

set(PDFEDIT_VERSION 0.1.0)
set(PDF4QT_VERSION 1.6.0.0)   # kept for upstream compatibility in vendored CMake
project(pdfedit VERSION ${PDFEDIT_VERSION} LANGUAGES CXX)

add_compile_definitions(PDF4QT_PROJECT_VERSION="${PDF4QT_VERSION}")
add_compile_definitions(QT_NO_EMIT)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PDFEDIT_BUILD_TESTS "Build UnitTests" ON)
option(PDFEDIT_INSTALL_INCLUDE "Install include headers" ON)

# vcpkg toolchain (deps: tbb openssl lcms zlib openjpeg freetype libjpeg-turbo
# libpng blend2d — see vcpkg.json; blend2d MUST come from vcpkg, see baseline doc)
if(DEFINED ENV{VCPKG_ROOT} AND NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
endif()

# Qt components used by core + CLI (no Widgets — headless)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Xml Svg Test)

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
add_subdirectory(PdfTool)

if(PDFEDIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(UnitTests)
endif()
