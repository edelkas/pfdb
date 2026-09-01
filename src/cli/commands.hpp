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
    bool dry_run = false;                // fetch/build but do not save
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

int cmd_init(const GlobalOptions& opts);
int cmd_add(const GlobalOptions& opts, const AddArgs& args);
int cmd_list(const GlobalOptions& opts);
int cmd_remove(const GlobalOptions& opts, Id id);
int cmd_search(const GlobalOptions& opts, const SearchArgs& args);
int cmd_update(const GlobalOptions& opts, const UpdateArgs& args);

}  // namespace pfdb::cli
