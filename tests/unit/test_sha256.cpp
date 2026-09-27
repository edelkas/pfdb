#include <catch2/catch_test_macros.hpp>

#include <string>

#include "util/sha256.hpp"

using pfdb::util::sha256_hex;

TEST_CASE("sha256 matches known single-block vectors", "[sha256]") {
    REQUIRE(sha256_hex("") ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    REQUIRE(sha256_hex("abc") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    REQUIRE(sha256_hex("The quick brown fox jumps over the lazy dog") ==
            "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
}

TEST_CASE("sha256 handles multi-block input", "[sha256]") {
    // 56 bytes forces the message length past a single 64-byte block.
    REQUIRE(sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    REQUIRE(sha256_hex(std::string(1000, 'a')).size() == 64);
}
