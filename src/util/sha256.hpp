#pragma once

#include <string>
#include <string_view>

namespace pfdb::util {

/// Compute the SHA-256 digest of `data` and return it as a lowercase hex string
/// (64 characters). Self-contained (no OpenSSL dependency); used to verify the
/// integrity of downloaded release archives against the update manifest.
std::string sha256_hex(std::string_view data);

}  // namespace pfdb::util
