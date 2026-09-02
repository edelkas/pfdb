#include "cli/commands.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "app/enrichment.hpp"
#include "db/repository.hpp"
#include "io/csv.hpp"
#include "io/film_json.hpp"
#include "model/collection_model.hpp"
#include "net/http_client.hpp"
#include "pfdb/film.hpp"
#include "query/engine.hpp"
#include "query/parser.hpp"
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

/// Fetch one source by external id (throws SourceError on failure).
sources::SourceFetch fetch_one(net::IHttpClient& http, const std::string& source_id,
                               const std::string& external_id) {
    std::unique_ptr<sources::ISource> source = sources::make_source(source_id, http);
    if (!source) {
        throw sources::SourceError(sources::SourceError::Kind::Network,
                                   "unknown source '" + source_id + "'");
    }
    return source->fetch(external_id);
}

/// A film built from one or more sources, plus the FilmAffinity edge data that
/// must be resolved to collection films after the film is saved.
struct BuiltFilm {
    Film film;
    std::vector<sources::RelatedRef> relations;
    std::vector<sources::SimilarRef> similars;
};

/// Fetch every source id present in `imdb_id`/`fa_id`, merge them, and return the
/// combined film + edges. On failure prints a message, sets `exit_code`, returns
/// nullopt. `apply_user` layers the manual `add` flags on top (skip for update).
std::optional<BuiltFilm> build_from_sources(net::IHttpClient& http,
                                            const std::optional<std::string>& imdb_id,
                                            const std::optional<std::string>& fa_id,
                                            int& exit_code) {
    try {
        std::optional<Film> imdb_film;
        std::optional<Film> fa_film;
        BuiltFilm built;
        if (imdb_id.has_value()) {
            sources::SourceFetch f = fetch_one(http, "imdb", *imdb_id);
            imdb_film = std::move(f.film);
        }
        if (fa_id.has_value()) {
            sources::SourceFetch f = fetch_one(http, "filmaffinity", *fa_id);
            fa_film = std::move(f.film);
            built.relations = std::move(f.relations);
            built.similars = std::move(f.similars);
        }
        built.film = app::merge_films(imdb_film, fa_film);
        return built;
    } catch (const sources::SourceError& e) {
        std::cerr << "error: " << e.what() << '\n';
        exit_code = exit_for(e);
        return std::nullopt;
    }
}

/// Resolve FilmAffinity edge refs to collection film ids (keeping only pairs
/// already in the database) and store them for `film_id`.
void store_edges(db::Repository& repo, Id film_id,
                 const std::vector<sources::RelatedRef>& relations,
                 const std::vector<sources::SimilarRef>& similars) {
    std::vector<db::Repository::RelationEdge> rel_edges;
    for (const auto& r : relations) {
        if (auto other = repo.find_id_by_source_ref("filmaffinity", r.external_id)) {
            rel_edges.push_back({*other, r.kind});
        }
    }
    repo.replace_relations(film_id, rel_edges);

    std::vector<db::Repository::SimilarityEdge> sim_edges;
    for (const auto& s : similars) {
        if (auto other = repo.find_id_by_source_ref("filmaffinity", s.external_id)) {
            sim_edges.push_back({*other, s.percent});
        }
    }
    repo.replace_similarities(film_id, sim_edges);
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
    const bool fetch_path = args.imdb_id.has_value() || args.fa_id.has_value();

    // Build the film either by fetching from source(s) or from manual flags.
    Film film;
    BuiltFilm built;
    if (fetch_path) {
        net::CprHttpClient http;
        int exit_code = kOk;
        auto result = build_from_sources(http, args.imdb_id, args.fa_id, exit_code);
        if (!result) {
            return exit_code;
        }
        built = std::move(*result);
        app::apply_overrides(built.film, overrides_from(args));
        film = built.film;
    } else {
        if (args.title.empty()) {
            std::cerr << "error: provide --title for a manual add, or --imdb/--fa "
                         "to fetch from a source\n";
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
        store_edges(*repo, id, built.relations, built.similars);
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

namespace {

/// A film as a JSON object, with its collection-resolved relation/similarity
/// edges attached (the same shape M3 emitted for the whole collection).
nlohmann::json film_json_with_edges(const db::Repository& repo,
                                    const CollectionModel& model, const Film& f) {
    nlohmann::json obj = to_json(f);
    nlohmann::json relations = nlohmann::json::array();
    for (const auto& e : repo.relations_of(f.id)) {
        const Film* other = model.find(e.other_id);
        relations.push_back({{"id", e.other_id},
                             {"title", other != nullptr ? other->title : ""},
                             {"kind", e.kind}});
    }
    obj["relations"] = std::move(relations);
    nlohmann::json similars = nlohmann::json::array();
    for (const auto& e : repo.similarities_of(f.id)) {
        const Film* other = model.find(e.other_id);
        similars.push_back({{"id", e.other_id},
                            {"title", other != nullptr ? other->title : ""},
                            {"percent", e.percent}});
    }
    obj["similarities"] = std::move(similars);
    return obj;
}

}  // namespace

int cmd_list(const GlobalOptions& opts, const ListArgs& args) {
    if (opts.json && args.csv) {
        std::cerr << "error: choose either --json or --csv, not both\n";
        return kUsageError;
    }
    auto repo = open_repo(opts);
    if (!repo) {
        return kRuntimeError;
    }
    const CollectionModel model = CollectionModel::load(*repo);

    // Parse, normalize, and run the query (filters + boolean expr + sort). With
    // no options this returns every film in load order.
    std::vector<const Film*> results;
    try {
        results = query::run_query(
            model, query::QueryRequest{args.filters, args.where, args.sort});
    } catch (const query::QueryError& e) {
        std::cerr << "error: " << e.what() << '\n';
        return kUsageError;
    }

    if (args.csv) {
        write_csv(std::cout, results);
    } else if (opts.json) {
        nlohmann::json arr = nlohmann::json::array();
        for (const Film* f : results) {
            arr.push_back(film_json_with_edges(*repo, model, *f));
        }
        std::cout << arr.dump(2) << '\n';
    } else {
        for (const Film* f : results) {
            print_film_row(std::cout, *f);
        }
        std::cerr << results.size() << " film(s).\n";
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

namespace {

std::optional<std::string> ref_for(const Film& film, const std::string& source) {
    for (const auto& ref : film.source_refs) {
        if (ref.source == source) {
            return ref.external_id;
        }
    }
    return std::nullopt;
}

/// Re-fetch one film's sources and update it in place, preserving all
/// user-specific data. Returns an exit code; `updated` is incremented on change.
int update_one(db::Repository& repo, net::IHttpClient& http, const Film& existing,
               int& updated) {
    const auto imdb_id = ref_for(existing, "imdb");
    const auto fa_id = ref_for(existing, "filmaffinity");
    if (!imdb_id.has_value() && !fa_id.has_value()) {
        std::cerr << "note: film #" << existing.id
                  << " has no online sources; skipping\n";
        return kOk;
    }

    int exit_code = kOk;
    auto result = build_from_sources(http, imdb_id, fa_id, exit_code);
    if (!result) {
        return exit_code;
    }

    // Refresh sourced fields but keep the user's own data untouched.
    Film refreshed = std::move(result->film);
    refreshed.id = existing.id;
    refreshed.created_at = existing.created_at;
    refreshed.user = existing.user;
    refreshed.video = existing.video;

    if (!repo.update(refreshed)) {
        std::cerr << "error: failed to update film #" << existing.id << '\n';
        return kRuntimeError;
    }
    store_edges(repo, existing.id, result->relations, result->similars);
    ++updated;
    return kOk;
}

}  // namespace

int cmd_update(const GlobalOptions& opts, const UpdateArgs& args) {
    if (!args.all && args.id == kInvalidId) {
        std::cerr << "error: provide a film id, or --all to update everything\n";
        return kUsageError;
    }
    auto repo = open_repo(opts);
    if (!repo) {
        return kRuntimeError;
    }

    std::vector<Film> targets;
    if (args.all) {
        targets = repo->load_all();
    } else {
        auto film = repo->find(args.id);
        if (!film) {
            std::cerr << "error: no film with id " << args.id << '\n';
            return kNotFound;
        }
        targets.push_back(std::move(*film));
    }

    net::CprHttpClient http;
    int updated = 0;
    int worst = kOk;
    for (const auto& film : targets) {
        const int rc = update_one(*repo, http, film, updated);
        if (rc != kOk) {
            worst = rc;  // keep going, but remember the failure
        }
    }
    std::cerr << "Updated " << updated << " film(s).\n";
    return worst;
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
