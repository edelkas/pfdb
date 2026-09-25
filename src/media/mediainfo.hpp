#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "pfdb/video_file.hpp"

namespace pfdb::media {

/// Probe a local video file with MediaInfo, returning its technical metadata
/// (general + video stream + audio/subtitle tracks). Throws std::runtime_error
/// if the file cannot be opened or parsed. `VideoFileInfo::path` is set to `path`.
VideoFileInfo probe(const std::string& path);

// --- Pure value helpers (exposed for unit testing) ---

/// Parse a leading integer out of a MediaInfo value that may carry grouping
/// spaces or a unit ("1 920", "5 000 000", "48000 Hz"), stopping at a '.'.
std::optional<long long> parse_int_loose(std::string_view s);

/// Parse a leading decimal ("23.976", "6120000"), tolerating grouping spaces.
std::optional<double> parse_double_loose(std::string_view s);

}  // namespace pfdb::media
