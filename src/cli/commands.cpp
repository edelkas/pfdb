#include "cli/commands.hpp"

#include <exception>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "db/repository.hpp"
#include "io/film_json.hpp"
#include "model/collection_model.hpp"
#include "pfdb/film.hpp"

namespace pfdb::cli {
namespace {

/// Open the repository, reporting failures uniformly. Returns nullopt on error
/// (after printing a message), so callers can `return kRuntimeError`.
std::optional<db::Repository> open_repo(const GlobalOptions& opts) {
    try {
        return db::Repository(opts.db_path);
    } catch (const std::exception& e) {
        std::cerr << "error: could not open database '" << opts.db_path
                  << "': " << e.what() << '\n';
        return std::nullopt;
    }
}

Film to_film(const AddArgs& args) {
    Film f;
    f.title = args.title;
    f.original_title = args.original_title;
    f.year = args.year;
    f.runtime_minutes = args.runtime_minutes;
    f.synopsis = args.synopsis;
    f.genres = args.genres;
    f.user.date_watched = args.date_watched;
    f.user.personal_rating = args.personal_rating;
    f.user.notes = args.notes;
    f.user.favorite = args.favorite;
    return f;
}

/// A compact one-line human summary of a film for table output.
void print_film_row(std::ostream& os, const Film& f) {
    os << f.id << '\t' << f.title;
    if (f.year.has_value()) {
        os << " (" << *f.year << ')';
    }
    if (f.user.favorite) {
        os << "  \xe2\x98\x85";  // star
    }
    os << '\n';
}

}  // namespace

int cmd_init(const GlobalOptions& opts) {
    auto repo = open_repo(opts);
    if (!repo) {
        return kRuntimeError;
    }
    if (opts.json) {
        nlohmann::json j{{"database", opts.db_path},
                         {"schema_version", repo->schema_version()},
                         {"films", repo->count()}};
        std::cout << j.dump(2) << '\n';
    } else {
        std::cout << "Initialized PFDB database at '" << opts.db_path << "' (schema v"
                  << repo->schema_version() << ").\n";
    }
    return kOk;
}

int cmd_add(const GlobalOptions& opts, const AddArgs& args) {
    auto repo = open_repo(opts);
    if (!repo) {
        return kRuntimeError;
    }
    try {
        const Id id = repo->insert(to_film(args));
        const auto stored = repo->find(id);
        if (opts.json && stored) {
            std::cout << to_json(*stored).dump(2) << '\n';
        } else {
            std::cout << "Added film #" << id << ": " << args.title << '\n';
        }
        return kOk;
    } catch (const std::exception& e) {
        std::cerr << "error: failed to add film: " << e.what() << '\n';
        return kRuntimeError;
    }
}

int cmd_list(const GlobalOptions& opts) {
    auto repo = open_repo(opts);
    if (!repo) {
        return kRuntimeError;
    }
    const CollectionModel model = CollectionModel::load(*repo);

    if (opts.json) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& f : model.all()) {
            arr.push_back(to_json(f));
        }
        std::cout << arr.dump(2) << '\n';
    } else {
        for (const auto& f : model.all()) {
            print_film_row(std::cout, f);
        }
        std::cerr << model.size() << " film(s).\n";
    }
    return kOk;
}

int cmd_remove(const GlobalOptions& opts, Id id) {
    auto repo = open_repo(opts);
    if (!repo) {
        return kRuntimeError;
    }
    try {
        if (!repo->remove(id)) {
            std::cerr << "error: no film with id " << id << '\n';
            return kNotFound;
        }
        if (opts.json) {
            std::cout << nlohmann::json{{"removed", id}}.dump(2) << '\n';
        } else {
            std::cout << "Removed film #" << id << ".\n";
        }
        return kOk;
    } catch (const std::exception& e) {
        std::cerr << "error: failed to remove film: " << e.what() << '\n';
        return kRuntimeError;
    }
}

}  // namespace pfdb::cli
