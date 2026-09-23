#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pfdb::config {

/// User-level configuration, persisted as JSON. Currently it holds named field
/// presets used by import/export; the shape is `{ "presets": { "<name>":
/// ["title","year", ...] } }`. It is deliberately independent of the collection
/// database so presets are shared across collections.
class Config {
public:
    Config() = default;

    /// The default config path: `$PFDB_CONFIG`, else `<home>/.pfdb/config.json`
    /// (`%USERPROFILE%` on Windows, `$HOME` elsewhere), else `pfdb.config.json`.
    static std::string default_path();

    /// Load the config at `path`. A missing file yields an empty config (with the
    /// path remembered so a later save() writes there). Throws std::runtime_error
    /// only if the file exists but is malformed.
    static Config load(const std::string& path);

    /// The tokens of a user-defined preset, or nullopt if `name` is not defined.
    std::optional<std::vector<std::string>> preset(const std::string& name) const;

    /// All user-defined presets (name -> tokens), sorted by name.
    const std::map<std::string, std::vector<std::string>>& presets() const {
        return presets_;
    }

    /// Define or replace a user preset.
    void set_preset(const std::string& name, std::vector<std::string> tokens);
    /// Remove a user preset; returns whether one existed.
    bool remove_preset(const std::string& name);

    /// Write the config back to its path, creating parent directories as needed.
    void save() const;

    const std::string& path() const { return path_; }

private:
    std::string path_;
    std::map<std::string, std::vector<std::string>> presets_;
};

}  // namespace pfdb::config
