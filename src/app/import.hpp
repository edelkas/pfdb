#pragma once

#include <string>

#include "io/field_set.hpp"

namespace pfdb::db {
class Repository;
}

namespace pfdb::app {

/// Outcome of an import run.
struct ImportStats {
    int added = 0;      ///< New films inserted.
    int updated = 0;    ///< Existing films (matched by IMDb id) whose selected fields were refreshed.
    int total = 0;      ///< Films parsed from the file.
};

/// Import films from an EMDB `emdb.dat` file, populating only `selection`.
///
/// Films are keyed by their IMDb id: if one already exists in the collection,
/// only the selected fields are copied onto it (everything else is preserved);
/// otherwise a new film is inserted. A parsed film without an IMDb id (or when
/// `imdb-id` isn't selected) is always inserted. With `dry_run`, nothing is
/// written and the stats report what *would* happen. Throws on read/parse errors.
ImportStats import_emdb(db::Repository& repo, const std::string& path,
                        const io::FieldSet& selection, bool dry_run);

}  // namespace pfdb::app
