#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "app/config.hpp"
#include "app/version.hpp"
#include "net/http_client.hpp"

namespace pfdb::app {

/// Thrown by the update pipeline on a malformed manifest or a failed step.
class UpdateError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// One downloadable build in the release manifest.
struct ReleaseAsset {
    std::string platform;   // e.g. "windows-x64"
    std::string archive;    // asset filename, e.g. "pfdb-0.2.0-windows-x64.zip"
    std::string url;        // absolute download URL
    std::string sha256;     // lowercase hex digest of the archive
    std::int64_t size = 0;  // archive size in bytes
    std::string cli_exe = "pfdb.exe";       // exe name inside the archive
    std::string gui_exe = "pfdb-gui.exe";   // exe name inside the archive
};

/// The parsed release manifest (a JSON asset attached to each GitHub Release).
struct UpdateManifest {
    SemVer version;
    std::string version_str;  // as written in the manifest
    std::string released;     // ISO date
    std::string notes;        // free-form update notes
    std::vector<ReleaseAsset> assets;
};

// --- Pure helpers (unit-tested) ---------------------------------------------

/// Parse a manifest JSON document. Throws UpdateError if it is malformed or the
/// version is unparseable.
UpdateManifest parse_manifest(std::string_view json);

/// The platform token for this build ("windows-x64", "linux-x64", ...).
std::string current_platform();

/// The staging directory PFDB downloads/unpacks into, beside the install dir.
/// Shared by the download and the first-run cleanup so they agree on the path.
std::string default_staging_dir(const std::string& install_dir);

/// The asset matching `platform`, or nullptr if the manifest has none.
const ReleaseAsset* select_asset(const UpdateManifest& m, std::string_view platform);

/// Whether the manifest describes a version newer than `current`.
bool is_newer(const UpdateManifest& m, const SemVer& current);

/// Whether `bytes` matches the asset's declared size and SHA-256.
bool verify_integrity(std::string_view bytes, const ReleaseAsset& asset);

// --- Networking (thin; covered by a hidden live test) -----------------------

/// GET the latest release manifest for `repo` ("owner/name"). Returns nullopt on
/// any network/HTTP failure so callers can silently skip an auto-check.
std::optional<UpdateManifest> fetch_manifest(net::IHttpClient& http,
                                             const std::string& repo);

// --- Orchestration ----------------------------------------------------------

/// A verified, unpacked update ready to be swapped into place.
struct PreparedUpdate {
    std::string version_str;
    std::string notes;
    std::string staging_dir;   // root staging directory (to clean up later)
    std::string unpacked_dir;  // where the new files live
    std::string cli_exe;
    std::string gui_exe;
};

/// Outcome of preparing an update (checking, and optionally downloading).
struct PrepareResult {
    bool up_to_date = false;         // already on the newest version
    bool available = false;          // a newer version exists
    std::string available_version;   // set when `available`
    std::string notes;               // update notes (when available)
    std::optional<PreparedUpdate> prepared;  // set when download+verify succeeded
    std::string error;               // non-empty on failure
};

/// Check for a newer version and, when `download` is true, download+verify+unpack
/// it into a fresh staging dir under `staging_root`, running a `--version` sanity
/// check on the extracted binaries. Never touches the live install directory.
PrepareResult prepare_update(net::IHttpClient& http, const std::string& repo,
                             const SemVer& current, const std::string& staging_root,
                             bool download);

/// Which executable the swapper should relaunch after swapping.
enum class Relaunch { None, Cli, Gui };

/// Launch the swapper: re-exec the *staged* CLI with the hidden `__apply-update`
/// subcommand so it can replace the install directory after this process exits.
/// Returns false if the swapper process could not be started.
bool launch_swapper(const PreparedUpdate& update, const std::string& install_dir,
                    const std::string& config_path, Relaunch relaunch);

/// Parameters for the in-place swap, parsed from the hidden `__apply-update`
/// subcommand's flags.
struct SwapParams {
    std::string from_dir;      // staging/unpacked
    std::string to_dir;        // install directory
    long wait_pid = 0;         // process to wait for before swapping
    std::string relaunch;      // "cli" | "gui" | "none"
    std::string version;       // the version being installed
    std::string notes_file;    // path to a file holding the update notes
    std::string config_path;   // where to record pending success/error
};

/// Run the swapper steps (wait → rename old → move new → record → relaunch).
/// Returns a process exit code. Intended to be called by `pfdb __apply-update`.
int run_swapper(const SwapParams& p);

/// Called on normal startup. Detects a just-applied update (or a failure the
/// swapper recorded), returns the notes/error to surface once, cleans up any
/// leftover `.old` files and staging, and records the current version.
struct FinalizeResult {
    bool updated = false;    // succeeded onto this version
    std::string notes;       // success notes to show once
    std::string error;       // failure message to show once
};
FinalizeResult finalize_update(config::Config& cfg, const std::string& install_dir);

}  // namespace pfdb::app
