#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pfdb {

/// One audio stream inside a local video file.
struct AudioTrack {
    std::string name;      ///< Track title, if any.
    std::string language;  ///< Language name or code, if available.
    std::string codec;     ///< Audio format, e.g. "AC-3", "DTS".
    std::optional<std::int64_t> size_bytes;
    std::optional<int> bitrate;      ///< Bits per second.
    std::optional<int> channels;     ///< Channel count.
    std::optional<int> sample_rate;  ///< Hz.

    friend bool operator==(const AudioTrack&, const AudioTrack&) = default;
};

/// One subtitle stream inside a local video file.
struct SubtitleTrack {
    std::string name;      ///< Track title, if any.
    std::string language;  ///< Language name or code, if available.
    std::string format;    ///< Subtitle format, e.g. "SRT", "PGS".
    std::optional<std::int64_t> size_bytes;

    friend bool operator==(const SubtitleTrack&, const SubtitleTrack&) = default;
};

/// Metadata about the local video file backing a film, when the user has one.
/// Most fields are optional because a film may have no file, or only partial
/// information may be known. The audio/subtitle track lists are filled by a
/// MediaInfo probe (see `pfdb scan`).
struct VideoFileInfo {
    /// Absolute or user-relative path to the video file.
    std::string path;
    std::optional<std::int64_t> size_bytes;
    std::optional<int> duration_seconds;
    std::optional<int> width;
    std::optional<int> height;
    /// Video codec / container description, e.g. "AVC", "HEVC".
    std::string codec;
    std::optional<double> framerate;    ///< Frames per second.
    std::optional<int> video_bitrate;   ///< Video stream bitrate, bits per second.
    std::vector<AudioTrack> audio_tracks;
    std::vector<SubtitleTrack> subtitle_tracks;

    friend bool operator==(const VideoFileInfo&, const VideoFileInfo&) = default;
};

}  // namespace pfdb
