#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "io/zip.hpp"

namespace fs = std::filesystem;

namespace {

std::string read_all(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

}  // namespace

TEST_CASE("extract_zip writes all entries, including nested dirs", "[zip]") {
    const std::string zip = std::string(PFDB_FIXTURES_DIR) + "/zip/sample.zip";
    const fs::path dest = fs::temp_directory_path() / "pfdb_zip_test";
    fs::remove_all(dest);

    pfdb::io::extract_zip(zip, dest.string());

    REQUIRE(fs::exists(dest / "hello.txt"));
    REQUIRE(read_all(dest / "hello.txt") == "Hello, PFDB!\n");
    REQUIRE(fs::exists(dest / "sub" / "nested.txt"));
    REQUIRE(read_all(dest / "sub" / "nested.txt") == "nested\n");

    fs::remove_all(dest);
}

TEST_CASE("extract_zip throws on a missing archive", "[zip]") {
    REQUIRE_THROWS(pfdb::io::extract_zip("does-not-exist.zip",
                                         fs::temp_directory_path().string()));
}
