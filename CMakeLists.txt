# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 albdf contributors
#
# Root build shim for the albdf repository.
#
# The authoritative CMake project is src/CMakeLists.txt: the fork of PDF4QT
# keeps its upstream-derived layout rooted at src/. This thin shim lets
# standard tooling and IDEs configure from the repository root:
#
#     cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
#           -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
#     cmake --build build
#
# The legacy invocation from inside src/ keeps working unchanged:
#
#     cmake -S src -B src/build ...
#
# In both cases generated headers and binaries land under the chosen build
# directory (${CMAKE_BINARY_DIR}); the subdirectories derive their include and
# output paths from it, so no other file needs to change.

cmake_minimum_required(VERSION 3.25)

project(albdf LANGUAGES CXX)

# Enable CTest at the top level so `ctest --test-dir build` discovers the tests
# that src/UnitTests registers when configuring from the repository root.
enable_testing()

add_subdirectory(src)
