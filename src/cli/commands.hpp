#pragma once

#include <optional>
#include <string>
#include <vector>

#include "pfdb/types.hpp"

namespace pfdb::cli {

/// Process-wide exit codes. Kept small and stable so scripts can branch on
/// them; CLI11 itself uses these conventions for usage errors too.
enum ExitCode : int {
    kOk = 0,
    kUsageError = 1,   // bad arguments (produced by CLI11)
    kRuntimeError = 2,  // I/O, database, or unexpected failure
    kNotFound = 3,      // referenced film/record does not exist
};

/// Options shared by every subcommand.
struct GlobalOptions {
    std::string db_path;
    bool json = false;
    /// Path to the user config (presets). Empty ⇒ resolve the default location.
    std::string config_path;
};

/// Fields accepted by `pfdb add`. Optionals are left unset when the user did
/// not supply the corresponding flag.
struct AddArgs {
    std::string title;
    std::string original_title;
    std::optional<int> year;
    std::optional<int> runtime_minutes;
    std::string synopsis;
    std::vector<std::string> genres;
    std::optional<std::string> date_watched;
    std::optional<double> personal_rating;
    std::string notes;
    bool favorite = false;

    // Online-fetch path. When an external id is given the film is fetched from a
    // source and the manual fields above are layered on top of the result.
    // Both may be given together to combine sources (IMDb wins shared fields).
    std::optional<std::string> imdb_id;  // --imdb <ttID>
    std::optional<std::string> fa_id;    // --fa <faId>
    bool financials = false;             // --financials: also fetch BoxOfficeMojo
    bool cover = false;                  // --cover: download & store cover art
    bool dry_run = false;                // fetch/build but do not save
};

/// Fields accepted by `pfdb scan` (MediaInfo probe of a local file).
struct ScanArgs {
    Id id = kInvalidId;
    std::string file;  // --file <path>; when empty, the film's stored path is used
};

/// Fields accepted by `pfdb cover`.
struct CoverArgs {
    Id id = kInvalidId;
    std::string out;  // --out <file>: export the stored cover
    std::string set;  // --set <file>: set the cover from a local file
};

/// Fields accepted by `pfdb update`.
struct UpdateArgs {
    Id id = kInvalidId;   // film to update (ignored if `all`)
    bool all = false;     // update every film in the collection
};

/// Fields accepted by `pfdb search`.
struct SearchArgs {
    std::string query;
    std::string source = "imdb";
};

/// Fields accepted by `pfdb list`. All are optional; with none given, `list`
/// prints the whole collection in load order, as before.
struct ListArgs {
    std::vector<std::string> filters;  // --filter/-f (repeatable): F1, F2, ...
    std::string where;                 // --where/-w: boolean expression
    std::string sort;                  // --sort/-s: field[:dir],...
    bool csv = false;                  // --csv: RFC-4180 output
    std::string fields;                // --fields: field token CSV (CSV export)
    std::string preset;                // --preset: named field selection
};

/// Fields accepted by `pfdb import`.
struct ImportArgs {
    std::string emdb;    // --emdb <path>: source EMDB .dat file
    std::string preset;  // --preset <name>
    std::string fields;  // --fields <csv>
    bool dry_run = false;
};

/// Fields accepted by `pfdb preset <action>`.
struct PresetArgs {
    enum class Action { List, Show, Set, Remove };
    Action action = Action::List;
    std::string name;
    std::string fields;  // for `set`
};

int cmd_init(const GlobalOptions& opts);
int cmd_add(const GlobalOptions& opts, const AddArgs& args);
int cmd_list(const GlobalOptions& opts, const ListArgs& args);
int cmd_remove(const GlobalOptions& opts, Id id);
int cmd_search(const GlobalOptions& opts, const SearchArgs& args);
int cmd_update(const GlobalOptions& opts, const UpdateArgs& args);
int cmd_import(const GlobalOptions& opts, const ImportArgs& args);
int cmd_preset(const GlobalOptions& opts, const PresetArgs& args);
int cmd_scan(const GlobalOptions& opts, const ScanArgs& args);
int cmd_cover(const GlobalOptions& opts, const CoverArgs& args);
int cmd_play(const GlobalOptions& opts, Id id);

}  // namespace pfdb::cli
