#include <catch2/catch_test_macros.hpp>

#include <string>

#include "app/update.hpp"
#include "app/version.hpp"

using namespace pfdb::app;

namespace {

const char* kManifest = R"({
  "version": "0.2.0",
  "released": "2026-09-27",
  "notes": "Adds self-update.",
  "assets": [
    {"platform": "windows-x64", "archive": "pfdb-0.2.0-windows-x64.zip",
     "url": "https://example.test/pfdb.zip", "size": 3,
     "sha256": "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
     "cli_exe": "pfdb.exe", "gui_exe": "pfdb-gui.exe"},
    {"platform": "linux-x64", "archive": "pfdb-0.2.0-linux-x64.zip",
     "size": 9, "sha256": "deadbeef"}
  ]
})";

}  // namespace

TEST_CASE("parse_manifest reads the full document", "[update]") {
    const UpdateManifest m = parse_manifest(kManifest);
    REQUIRE(m.version_str == "0.2.0");
    REQUIRE(m.version == *parse_semver("0.2.0"));
    REQUIRE(m.released == "2026-09-27");
    REQUIRE(m.notes == "Adds self-update.");
    REQUIRE(m.assets.size() == 2);
    // sha256 is normalised to lowercase for comparison.
    REQUIRE(m.assets[0].sha256 ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    REQUIRE(m.assets[0].cli_exe == "pfdb.exe");
    REQUIRE(m.assets[1].cli_exe == "pfdb.exe");  // default when omitted
}

TEST_CASE("parse_manifest tolerates missing optionals", "[update]") {
    const UpdateManifest m = parse_manifest(
        R"({"version":"1.0.0","assets":[{"platform":"p","archive":"a.zip"}]})");
    REQUIRE(m.version_str == "1.0.0");
    REQUIRE(m.notes.empty());
    REQUIRE(m.assets.size() == 1);
}

TEST_CASE("parse_manifest rejects malformed input", "[update]") {
    REQUIRE_THROWS_AS(parse_manifest("not json"), UpdateError);
    REQUIRE_THROWS_AS(parse_manifest("{}"), UpdateError);            // no version
    REQUIRE_THROWS_AS(parse_manifest(R"({"version":"x"})"), UpdateError);  // bad semver
}

TEST_CASE("select_asset matches by platform", "[update]") {
    const UpdateManifest m = parse_manifest(kManifest);
    const ReleaseAsset* win = select_asset(m, "windows-x64");
    REQUIRE(win != nullptr);
    REQUIRE(win->archive == "pfdb-0.2.0-windows-x64.zip");
    REQUIRE(select_asset(m, "solaris-sparc") == nullptr);
}

TEST_CASE("is_newer compares against the current version", "[update]") {
    const UpdateManifest m = parse_manifest(kManifest);  // 0.2.0
    REQUIRE(is_newer(m, *parse_semver("0.1.0")));
    REQUIRE_FALSE(is_newer(m, *parse_semver("0.2.0")));
    REQUIRE_FALSE(is_newer(m, *parse_semver("0.3.0")));
    REQUIRE(is_newer(m, *parse_semver("0.2.0-rc.1")));  // release beats pre-release
}

TEST_CASE("verify_integrity checks size and checksum", "[update]") {
    const UpdateManifest m = parse_manifest(kManifest);
    const ReleaseAsset& win = m.assets[0];  // size 3, sha256 of "abc"
    REQUIRE(verify_integrity("abc", win));
    REQUIRE_FALSE(verify_integrity("abcd", win));  // wrong size
    REQUIRE_FALSE(verify_integrity("xyz", win));   // wrong checksum

    ReleaseAsset no_hash = win;
    no_hash.sha256.clear();
    REQUIRE_FALSE(verify_integrity("abc", no_hash));  // refuse without a checksum
}

TEST_CASE("current_platform is non-empty", "[update]") {
    REQUIRE_FALSE(current_platform().empty());
}
