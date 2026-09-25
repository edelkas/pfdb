#pragma once

#include <string>

namespace pfdb::app {

/// Open `path` in the operating system's default application (a video player,
/// for a film file). Returns whether the launch was initiated successfully.
bool open_in_default_app(const std::string& path);

}  // namespace pfdb::app
