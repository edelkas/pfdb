cmake_minimum_required(VERSION 3.25)

# ---------------------------------------------------------------------------
# PFDB - Power Film DataBase
#
# Top-level build. Three targets:
#   pfdb_core  - static library holding all logic (domain, db, model, ...)
#   pfdb_cli   - the command-line front-end executable
#   pfdb_tests - Catch2 test runner (only when PFDB_BUILD_TESTS is ON)
#
# Dependencies are provided by vcpkg (manifest mode). Configure through a
# CMake preset so the vcpkg toolchain file is wired in automatically.
# ---------------------------------------------------------------------------

project(pfdb
    VERSION 0.1.0
    DESCRIPTION "Power Film DataBase - a power-user film collection manager"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clang-tidy / editors.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PFDB_BUILD_TESTS "Build the PFDB test suite" ON)
option(PFDB_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

# ---------------------------------------------------------------------------
# Shared warning flags, applied to our own targets only (never to deps).
# ---------------------------------------------------------------------------
add_library(pfdb_warnings INTERFACE)
if(MSVC)
    target_compile_options(pfdb_warnings INTERFACE /W4 /permissive- /EHsc)
    if(PFDB_WARNINGS_AS_ERRORS)
        target_compile_options(pfdb_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(pfdb_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(PFDB_WARNINGS_AS_ERRORS)
        target_compile_options(pfdb_warnings INTERFACE -Werror)
    endif()
endif()

# ---------------------------------------------------------------------------
# Dependencies (from vcpkg).
# ---------------------------------------------------------------------------
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(lexbor CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Core library.
# ---------------------------------------------------------------------------
add_subdirectory(src)

# ---------------------------------------------------------------------------
# Tests.
# ---------------------------------------------------------------------------
if(PFDB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
