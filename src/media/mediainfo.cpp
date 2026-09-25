#include "media/mediainfo.hpp"

#include <stdexcept>

// The vcpkg MediaInfo build ships as a DLL that exports only the C API (the C++
// MediaInfoLib::MediaInfo class isn't dllexported), so use the DLL wrapper's
// inline class, statically bound to the import library.
#include <MediaInfoDLL/MediaInfoDLL_Static.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace pfdb::media {
namespace {

using MediaInfoDLL::MediaInfo;
using MediaInfoDLL::Stream_Audio;
using MediaInfoDLL::Stream_General;
using MediaInfoDLL::Stream_Text;
using MediaInfoDLL::Stream_Video;
using MediaInfoDLL::String;

#ifdef _WIN32
// UTF-8 <-> UTF-16 (MediaInfo's String is std::wstring on Windows).
String to_native(const std::string& s) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::wstring w(static_cast<std::size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}
std::string from_native(const String& w) {
    const int n =
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) {
        return {};
    }
    std::string s(static_cast<std::size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}
#else
// Elsewhere MediaInfo's String is a UTF-8 std::string already.
String to_native(const std::string& s) { return s; }
std::string from_native(const String& s) { return s; }
#endif

std::string get(MediaInfo& mi, MediaInfoDLL::stream_t kind, std::size_t number,
                const char* param) {
    return from_native(mi.Get(kind, number, to_native(param)));
}

std::optional<int> to_int(std::optional<long long> v) {
    if (!v) {
        return std::nullopt;
    }
    return static_cast<int>(*v);
}

}  // namespace

std::optional<long long> parse_int_loose(std::string_view s) {
    std::string digits;
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            digits.push_back(c);
        } else if (c == ' ') {
            continue;  // grouping separator
        } else if (!digits.empty()) {
            break;  // reached a unit or a decimal point after the number
        } else if (c == '.' || c == '-') {
            break;
        }
    }
    if (digits.empty()) {
        return std::nullopt;
    }
    try {
        return std::stoll(digits);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<double> parse_double_loose(std::string_view s) {
    std::string num;
    bool seen_dot = false;
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            num.push_back(c);
        } else if (c == '.' && !seen_dot && !num.empty()) {
            seen_dot = true;
            num.push_back(c);
        } else if (c == ' ') {
            continue;
        } else if (!num.empty()) {
            break;
        }
    }
    if (num.empty()) {
        return std::nullopt;
    }
    try {
        return std::stod(num);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

VideoFileInfo probe(const std::string& path) {
    MediaInfo mi;
    if (mi.Open(to_native(path)) == 0) {
        throw std::runtime_error("mediainfo: could not open '" + path + "'");
    }

    VideoFileInfo v;
    v.path = path;
    v.size_bytes = parse_int_loose(get(mi, Stream_General, 0, "FileSize"));
    if (auto ms = parse_double_loose(get(mi, Stream_General, 0, "Duration"))) {
        v.duration_seconds = static_cast<int>(*ms / 1000.0);
    }

    if (mi.Count_Get(Stream_Video) > 0) {
        v.width = to_int(parse_int_loose(get(mi, Stream_Video, 0, "Width")));
        v.height = to_int(parse_int_loose(get(mi, Stream_Video, 0, "Height")));
        v.framerate = parse_double_loose(get(mi, Stream_Video, 0, "FrameRate"));
        v.video_bitrate = to_int(parse_int_loose(get(mi, Stream_Video, 0, "BitRate")));
        v.codec = get(mi, Stream_Video, 0, "Format");
    }

    for (std::size_t i = 0; i < mi.Count_Get(Stream_Audio); ++i) {
        AudioTrack a;
        a.name = get(mi, Stream_Audio, i, "Title");
        a.language = get(mi, Stream_Audio, i, "Language/String");
        a.codec = get(mi, Stream_Audio, i, "Format");
        a.size_bytes = parse_int_loose(get(mi, Stream_Audio, i, "StreamSize"));
        a.bitrate = to_int(parse_int_loose(get(mi, Stream_Audio, i, "BitRate")));
        a.channels = to_int(parse_int_loose(get(mi, Stream_Audio, i, "Channel(s)")));
        a.sample_rate = to_int(parse_int_loose(get(mi, Stream_Audio, i, "SamplingRate")));
        v.audio_tracks.push_back(std::move(a));
    }

    for (std::size_t i = 0; i < mi.Count_Get(Stream_Text); ++i) {
        SubtitleTrack s;
        s.name = get(mi, Stream_Text, i, "Title");
        s.language = get(mi, Stream_Text, i, "Language/String");
        s.format = get(mi, Stream_Text, i, "Format");
        s.size_bytes = parse_int_loose(get(mi, Stream_Text, i, "StreamSize"));
        v.subtitle_tracks.push_back(std::move(s));
    }

    mi.Close();
    return v;
}

}  // namespace pfdb::media
