#include "cli/commands.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "app/enrichment.hpp"
#include "db/repository.hpp"
#include "io/film_json.hpp"
#include "model/collection_model.hpp"
#include "net/http_client.hpp"
#include "pfdb/film.hpp"
#include "sources/source.hpp"
#include "sources/source_registry.hpp"

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

/// Translate the manual `add` flags into enrichment overrides layered on top of
/// a fetched film. Empty manual strings are treated as "not provided" so they
/// don't wipe fetched data.
app::FilmOverrides overrides_from(const AddArgs& args) {
    app::FilmOverrides ov;
    ov.favorite = args.favorite;
    ov.date_watched = args.date_watched;
    ov.personal_rating = args.personal_rating;
    if (!args.notes.empty()) {
        ov.notes = args.notes;
    }
    ov.add_genres = args.genres;
    if (!args.title.empty()) {
        ov.title = args.title;
    }
    if (args.year.has_value()) {
        ov.year = args.year;
    }
    return ov;
}

/// Map a SourceError to a CLI exit code (after the caller prints the message).
int exit_for(const sources::SourceError& e) {
    return e.kind() == sources::SourceError::Kind::NotFound ? kNotFound : kRuntimeError;
}

/// Fetch a film from a source by external id, applying manual overrides. On
/// failure prints a message and sets `exit_code`; returns nullopt.
std::optional<Film> fetch_film(const AddArgs& args, const std::string& source_id,
                               const std::string& external_id, int& exit_code) {
    net::CprHttpClient http;
    std::unique_ptr<sources::ISource> source = sources::make_source(source_id, http);
    if (!source) {
        std::cerr << "error: unknown source '" << source_id << "'\n";
        exit_code = kUsageError;
        return std::nullopt;
    }
    try {
        Film film = source->fetch(external_id);
        app::apply_overrides(film, overrides_from(args));
        return film;
    } catch (const sources::SourceError& e) {
        std::cerr << "error: " << e.what() << '\n';
        exit_code = exit_for(e);
        return std::nullopt;
    }
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
    // Build the film either by fetching from a source or from manual flags.
    Film film;
    if (args.imdb_id.has_value()) {
        int exit_code = kOk;
        std::optional<Film> fetched = fetch_film(args, "imdb", *args.imdb_id, exit_code);
        if (!fetched) {
            return exit_code;
        }
        film = std::move(*fetched);
    } else {
        if (args.title.empty()) {
            std::cerr << "error: provide --title for a manual add, "
                         "or --imdb <id> to fetch from IMDb\n";
            return kUsageError;
        }
        film = to_film(args);
    }

    if (args.dry_run) {
        if (opts.json) {
            std::cout << to_json(film).dump(2) << '\n';
        } else {
            std::cout << "Would add: " << film.title;
            if (film.year.has_value()) {
                std::cout << " (" << *film.year << ')';
            }
            std::cout << " [not saved]\n";
        }
        return kOk;
    }

    auto repo = open_repo(opts);
    if (!repo) {
        return kRuntimeError;
    }
    try {
        const Id id = repo->insert(film);
        const auto stored = repo->find(id);
        if (opts.json && stored) {
            std::cout << to_json(*stored).dump(2) << '\n';
        } else {
            std::cout << "Added film #" << id << ": " << film.title << '\n';
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

int cmd_search(const GlobalOptions& opts, const SearchArgs& args) {
    net::CprHttpClient http;
    std::unique_ptr<sources::ISource> source = sources::make_source(args.source, http);
    if (!source) {
        std::cerr << "error: unknown source '" << args.source << "'\n";
        return kUsageError;
    }

    std::vector<sources::SearchResult> results;
    try {
        results = source->search(args.query);
    } catch (const sources::SourceError& e) {
        std::cerr << "error: " << e.what() << '\n';
        return exit_for(e);
    }

    if (opts.json) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& r : results) {
            arr.push_back({
                {"source", r.source},
                {"id", r.external_id},
                {"title", r.title},
                {"year", r.year.has_value() ? nlohmann::json(*r.year) : nlohmann::json(nullptr)},
                {"type", r.type},
                {"subtitle", r.subtitle},
                {"image_url", r.image_url},
            });
        }
        std::cout << arr.dump(2) << '\n';
    } else {
        for (const auto& r : results) {
            std::cout << r.external_id << '\t' << r.title;
            if (r.year.has_value()) {
                std::cout << " (" << *r.year << ')';
            }
            if (!r.type.empty()) {
                std::cout << "  [" << r.type << ']';
            }
            std::cout << '\n';
        }
        std::cerr << results.size() << " result(s).\n";
    }
    return kOk;
}

}  // namespace pfdb::cli
