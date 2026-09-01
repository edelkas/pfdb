#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace pfdb::test {

/// Read a test fixture file (path relative to tests/fixtures/) as raw bytes.
/// PFDB_FIXTURES_DIR is provided by CMake (see tests/CMakeLists.txt).
inline std::string read_fixture(const std::string& relative_path) {
#ifndef PFDB_FIXTURES_DIR
#error "PFDB_FIXTURES_DIR must be defined by the build"
#endif
    const std::filesystem::path full =
        std::filesystem::path(PFDB_FIXTURES_DIR) / relative_path;
    std::ifstream in(full, std::ios::binary);
    if (!in) {
        throw std::runtime_error("fixture not found: " + full.string());
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace pfdb::test
