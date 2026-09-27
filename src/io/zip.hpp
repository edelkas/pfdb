#pragma once

#include <string>

namespace pfdb::io {

/// Extract every entry of the zip archive at `zip_path` into `dest_dir`,
/// creating subdirectories as needed. Existing files are overwritten. Throws
/// std::runtime_error if the archive cannot be opened or an entry fails to
/// extract. Used by the update service to unpack a downloaded release archive.
void extract_zip(const std::string& zip_path, const std::string& dest_dir);

}  // namespace pfdb::io
