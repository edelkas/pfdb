#include "app/config.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace pfdb::config {
namespace {

std::optional<std::string> env(const char* name) {
#ifdef _WIN32
    // std::getenv is flagged C4996 by MSVC (fatal under /WX); use the safe CRT.
    char* buf = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&buf, &len, name) != 0 || buf == nullptr) {
        return std::nullopt;
    }
    std::string value(buf);
    std::free(buf);
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
#else
    const char* v = std::getenv(name);
    if (v == nullptr || v[0] == '\0') {
        return std::nullopt;
    }
    return std::string(v);
#endif
}

}  // namespace

std::string Config::default_path() {
    if (auto p = env("PFDB_CONFIG")) {
        return *p;
    }
    std::optional<std::string> home = env("USERPROFILE");
    if (!home) {
        home = env("HOME");
    }
    if (home) {
        return (std::filesystem::path(*home) / ".pfdb" / "config.json").string();
    }
    return "pfdb.config.json";
}

Config Config::load(const std::string& path) {
    Config cfg;
    cfg.path_ = path;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return cfg;  // missing file -> empty config
    }
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    if (text.empty()) {
        return cfg;
    }
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(text);
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("invalid config '" + path + "': " + e.what());
    }
    if (auto it = j.find("presets"); it != j.end() && it->is_object()) {
        for (const auto& [name, arr] : it->items()) {
            std::vector<std::string> tokens;
            if (arr.is_array()) {
                for (const auto& t : arr) {
                    tokens.push_back(t.get<std::string>());
                }
            }
            cfg.presets_[name] = std::move(tokens);
        }
    }
    return cfg;
}

std::optional<std::vector<std::string>> Config::preset(const std::string& name) const {
    auto it = presets_.find(name);
    if (it == presets_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void Config::set_preset(const std::string& name, std::vector<std::string> tokens) {
    presets_[name] = std::move(tokens);
}

bool Config::remove_preset(const std::string& name) {
    return presets_.erase(name) > 0;
}

void Config::save() const {
    nlohmann::json presets_json = nlohmann::json::object();
    for (const auto& [name, tokens] : presets_) {
        presets_json[name] = tokens;
    }
    const nlohmann::json j{{"presets", std::move(presets_json)}};

    const std::filesystem::path p(path_);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("could not write config '" + path_ + "'");
    }
    out << j.dump(2) << '\n';
}

}  // namespace pfdb::config
