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
};

int cmd_init(const GlobalOptions& opts);
int cmd_add(const GlobalOptions& opts, const AddArgs& args);
int cmd_list(const GlobalOptions& opts);
int cmd_remove(const GlobalOptions& opts, Id id);

}  // namespace pfdb::cli
