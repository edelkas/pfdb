#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pfdb::config {

/// Self-update settings, persisted under the JSON key "update". Holds the check
/// cadence, bookkeeping for when the last check ran and which version last ran
/// (so a just-applied update can be detected on first launch), and one-shot
/// "pending" fields the swapper writes to surface a result modal after a swap.
struct UpdateSettings {
    int check_interval_hours = 24;    ///< 0 disables the automatic check.
    std::int64_t last_check = 0;      ///< Unix seconds of the last check; 0 = never.
    std::string last_run_version;     ///< Version that last completed a normal run.
    std::string pending_version;      ///< Version the swapper just installed.
    std::string pending_notes;        ///< Notes to show once after a successful swap.
    std::string pending_error;        ///< Error to show once after a failed swap.
    std::string repo = "edelkas/pfdb";  ///< GitHub "owner/name" for releases.
};

/// User-level configuration, persisted as JSON. It holds named field presets
/// used by import/export (shape `{ "presets": { "<name>": ["title", ...] } }`)
/// and self-update settings (`{ "update": { ... } }`). It is deliberately
/// independent of the collection database so it is shared across collections.
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

    /// The self-update settings (mutable / read-only).
    UpdateSettings& update() { return update_; }
    const UpdateSettings& update() const { return update_; }

    /// Whether an automatic update check is due at `now` (Unix seconds): true if
    /// the interval is positive and at least that many hours have elapsed since
    /// the last check (or none has ever run).
    bool update_check_due(std::int64_t now) const;

    /// Record that a check ran at `now` (does not persist; call save()).
    void mark_update_checked(std::int64_t now) { update_.last_check = now; }

    /// Write the config back to its path, creating parent directories as needed.
    void save() const;

    const std::string& path() const { return path_; }

private:
    std::string path_;
    std::map<std::string, std::vector<std::string>> presets_;
    UpdateSettings update_;
};

}  // namespace pfdb::config
