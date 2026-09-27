#include "app/update.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

#include "app/platform.hpp"
#include "io/zip.hpp"
#include "util/sha256.hpp"

namespace pfdb::app {
namespace fs = std::filesystem;
namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void write_file(const std::string& path, std::string_view data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

std::string download_url(const ReleaseAsset& asset, const std::string& repo) {
    if (!asset.url.empty()) {
        return asset.url;
    }
    return "https://github.com/" + repo + "/releases/latest/download/" + asset.archive;
}

}  // namespace

UpdateManifest parse_manifest(std::string_view json) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
    } catch (const nlohmann::json::exception& e) {
        throw UpdateError(std::string("malformed manifest: ") + e.what());
    }
    if (!j.is_object()) {
        throw UpdateError("manifest is not a JSON object");
    }

    UpdateManifest m;
    m.version_str = j.value("version", std::string{});
    if (m.version_str.empty()) {
        throw UpdateError("manifest has no version");
    }
    if (auto v = parse_semver(m.version_str)) {
        m.version = *v;
    } else {
        throw UpdateError("manifest version '" + m.version_str + "' is not semver");
    }
    m.released = j.value("released", std::string{});
    m.notes = j.value("notes", std::string{});

    if (auto it = j.find("assets"); it != j.end() && it->is_array()) {
        for (const auto& a : *it) {
            if (!a.is_object()) {
                continue;
            }
            ReleaseAsset asset;
            asset.platform = a.value("platform", std::string{});
            asset.archive = a.value("archive", a.value("file", std::string{}));
            if (asset.platform.empty() || asset.archive.empty()) {
                continue;  // unusable entry
            }
            asset.url = a.value("url", std::string{});
            asset.sha256 = to_lower(a.value("sha256", std::string{}));
            asset.size = a.value("size", static_cast<std::int64_t>(0));
            asset.cli_exe = a.value("cli_exe", std::string{"pfdb.exe"});
            asset.gui_exe = a.value("gui_exe", std::string{"pfdb-gui.exe"});
            m.assets.push_back(std::move(asset));
        }
    }
    return m;
}

std::string default_staging_dir(const std::string& install_dir) {
    return (fs::path(install_dir) / "pfdb-update").string();
}

std::string current_platform() {
    const bool is64 = sizeof(void*) == 8;
#if defined(_WIN32)
    return is64 ? "windows-x64" : "windows-x86";
#elif defined(__APPLE__)
#if defined(__aarch64__)
    return "macos-arm64";
#else
    return is64 ? "macos-x64" : "macos-x86";
#endif
#elif defined(__linux__)
#if defined(__aarch64__)
    return "linux-arm64";
#else
    return is64 ? "linux-x64" : "linux-x86";
#endif
#else
    return is64 ? "unknown-x64" : "unknown-x86";
#endif
}

const ReleaseAsset* select_asset(const UpdateManifest& m, std::string_view platform) {
    for (const auto& a : m.assets) {
        if (a.platform == platform) {
            return &a;
        }
    }
    return nullptr;
}

bool is_newer(const UpdateManifest& m, const SemVer& current) {
    return current < m.version;
}

bool verify_integrity(std::string_view bytes, const ReleaseAsset& asset) {
    if (asset.size > 0 && static_cast<std::int64_t>(bytes.size()) != asset.size) {
        return false;
    }
    if (asset.sha256.empty()) {
        return false;  // no checksum to verify against -> refuse
    }
    return to_lower(util::sha256_hex(bytes)) == asset.sha256;
}

std::optional<UpdateManifest> fetch_manifest(net::IHttpClient& http,
                                             const std::string& repo) {
    const std::string url =
        "https://github.com/" + repo + "/releases/latest/download/manifest.json";
    const net::HttpResponse res = http.get(url);
    if (!res.ok() || res.body.empty()) {
        return std::nullopt;
    }
    try {
        return parse_manifest(res.body);
    } catch (const UpdateError&) {
        return std::nullopt;
    }
}

PrepareResult prepare_update(net::IHttpClient& http, const std::string& repo,
                             const SemVer& current, const std::string& staging_root,
                             bool download) {
    PrepareResult r;
    std::optional<UpdateManifest> manifest = fetch_manifest(http, repo);
    if (!manifest) {
        r.error = "could not reach the update server";
        return r;
    }
    if (!is_newer(*manifest, current)) {
        r.up_to_date = true;
        return r;
    }

    r.available = true;
    r.available_version = manifest->version_str;
    r.notes = manifest->notes;
    if (!download) {
        return r;
    }

    const ReleaseAsset* asset = select_asset(*manifest, current_platform());
    if (asset == nullptr) {
        r.error = "no build available for " + current_platform();
        return r;
    }

    std::error_code ec;
    fs::remove_all(staging_root, ec);
    fs::create_directories(staging_root, ec);
    const std::string archive_path = (fs::path(staging_root) / asset->archive).string();
    const std::string unpacked = (fs::path(staging_root) / "unpacked").string();

    // Download.
    const net::HttpResponse res = http.get(download_url(*asset, repo));
    if (!res.ok() || res.body.empty()) {
        r.error = "failed to download the update";
        return r;
    }

    // Integrity: size + checksum.
    if (!verify_integrity(res.body, *asset)) {
        fs::remove_all(staging_root, ec);
        r.error = "integrity check failed (size/checksum mismatch); update discarded";
        return r;
    }
    write_file(archive_path, res.body);

    // Unpack.
    try {
        fs::create_directories(unpacked, ec);
        io::extract_zip(archive_path, unpacked);
    } catch (const std::exception& e) {
        fs::remove_all(staging_root, ec);
        r.error = std::string("failed to unpack the update: ") + e.what();
        return r;
    }

    // Version sanity check on the extracted binaries.
    auto check_exe = [&](const std::string& exe) -> bool {
        const std::string path = (fs::path(unpacked) / exe).string();
        if (!fs::exists(path)) {
            return false;
        }
        std::string out;
        const int code = run_and_capture(path, {"--version"}, out);
        return code == 0 && out.find(manifest->version_str) != std::string::npos;
    };
    if (!check_exe(asset->cli_exe) || !check_exe(asset->gui_exe)) {
        fs::remove_all(staging_root, ec);
        r.error = "the downloaded build failed its version check; update discarded";
        return r;
    }

    PreparedUpdate prepared;
    prepared.version_str = manifest->version_str;
    prepared.notes = manifest->notes;
    prepared.staging_dir = staging_root;
    prepared.unpacked_dir = unpacked;
    prepared.cli_exe = asset->cli_exe;
    prepared.gui_exe = asset->gui_exe;
    r.prepared = std::move(prepared);
    return r;
}

bool launch_swapper(const PreparedUpdate& update, const std::string& install_dir,
                    const std::string& config_path, Relaunch relaunch) {
    const std::string swapper_exe =
        (fs::path(update.unpacked_dir) / update.cli_exe).string();
    const std::string notes_file =
        (fs::path(update.staging_dir) / "notes.txt").string();
    write_file(notes_file, update.notes);

    std::string relaunch_str = "none";
    if (relaunch == Relaunch::Cli) {
        relaunch_str = "cli";
    } else if (relaunch == Relaunch::Gui) {
        relaunch_str = "gui";
    }

    const std::vector<std::string> args = {
        "__apply-update",
        "--from", update.unpacked_dir,
        "--to", install_dir,
        "--wait-pid", std::to_string(current_process_id()),
        "--relaunch", relaunch_str,
        "--version", update.version_str,
        "--notes-file", notes_file,
        "--config", config_path,
    };
    return launch_detached(swapper_exe, args);
}

int run_swapper(const SwapParams& p) {
    wait_for_pid(p.wait_pid);

    std::error_code ec;
    std::vector<std::pair<fs::path, fs::path>> renamed;  // {backup, original}
    bool failed = false;
    std::string fail_msg;

    auto rollback = [&]() {
        for (auto it = renamed.rbegin(); it != renamed.rend(); ++it) {
            fs::remove(it->second, ec);            // remove the (partial) new file
            fs::rename(it->first, it->second, ec);  // restore the .old backup
        }
    };

    try {
        for (const auto& entry : fs::recursive_directory_iterator(p.from_dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const fs::path rel = fs::relative(entry.path(), p.from_dir);
            const fs::path target = fs::path(p.to_dir) / rel;
            fs::create_directories(target.parent_path(), ec);

            if (fs::exists(target)) {
                const fs::path backup = fs::path(target) += ".old";
                fs::remove(backup, ec);
                fs::rename(target, backup);  // throws on failure
                renamed.emplace_back(backup, target);
            }
            fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing);
        }
    } catch (const std::exception& e) {
        failed = true;
        fail_msg = e.what();
        rollback();
    }

    // Record the outcome for the next launch's modal.
    config::Config cfg = config::Config::load(p.config_path);
    if (failed) {
        cfg.update().pending_error =
            "The update could not be applied and was rolled back (" + fail_msg + ").";
        cfg.update().pending_version.clear();
        cfg.update().pending_notes.clear();
    } else {
        cfg.update().pending_version = p.version;
        cfg.update().pending_notes = read_file(p.notes_file);
        cfg.update().pending_error.clear();
    }
    cfg.save();

    // Relaunch the (new or, on rollback, restored) executable.
    if (p.relaunch == "cli" || p.relaunch == "gui") {
        const std::string exe = (fs::path(p.to_dir) /
                                 (p.relaunch == "gui" ? "pfdb-gui.exe" : "pfdb.exe"))
                                    .string();
        launch_detached(exe, {});
    }
    return failed ? 1 : 0;
}

FinalizeResult finalize_update(config::Config& cfg, const std::string& install_dir) {
    FinalizeResult r;
    config::UpdateSettings& u = cfg.update();
    const std::string current = current_version().str();
    bool dirty = false;

    if (!u.pending_error.empty()) {
        r.error = u.pending_error;
        u.pending_error.clear();
        u.pending_version.clear();
        u.pending_notes.clear();
        dirty = true;
    } else if (!u.pending_version.empty()) {
        if (u.pending_version == current) {
            r.updated = true;
            r.notes = u.pending_notes;
        }
        u.pending_version.clear();
        u.pending_notes.clear();
        dirty = true;
    }

    if (u.last_run_version != current) {
        // A new version is running: clean up the previous binaries and staging.
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(install_dir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".old") {
                fs::remove(entry.path(), ec);
            }
        }
        fs::remove_all(default_staging_dir(install_dir), ec);
        u.last_run_version = current;
        dirty = true;
    }

    if (dirty) {
        try {
            cfg.save();
        } catch (const std::exception&) {
            // A read-only config must not break startup.
        }
    }
    return r;
}

}  // namespace pfdb::app
