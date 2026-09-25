#include "cli/commands.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "app/config.hpp"
#include "app/cover.hpp"
#include "app/enrichment.hpp"
#include "app/import.hpp"
#include "app/player.hpp"
#include "db/repository.hpp"
#include "io/csv.hpp"
#include "io/field_set.hpp"
#include "io/film_json.hpp"
#include "media/mediainfo.hpp"
#include "model/collection_model.hpp"
#include "net/http_client.hpp"
#include "pfdb/film.hpp"
#include "query/engine.hpp"
#include "query/parser.hpp"
#include "sources/source.hpp"
#include "sources/source_registry.hpp"

namespace pfdb::cli {
namespace {

/// Load the user config (presets), resolving the default path when unset.
config::Config load_config(const GlobalOptions& opts) {
    const std::string path =
        opts.config_path.empty() ? config::Config::default_path() : opts.config_path;
    return config::Config::load(path);
}

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
    std::string cover_url;  ///< Best cover URL across sources (IMDb preferred).
};

/// Fetch every requested source (imdb/fa/boxofficemojo), merge them, and return
/// the combined film + edges + cover url. On failure prints a message, sets
/// `exit_code`, returns nullopt.
std::optional<BuiltFilm> build_from_sources(net::IHttpClient& http,
                                            const std::optional<std::string>& imdb_id,
                                            const std::optional<std::string>& fa_id,
                                            const std::optional<std::string>& bom_id,
                                            int& exit_code) {
    try {
        std::optional<Film> imdb_film;
        std::optional<Film> fa_film;
        std::optional<Film> bom_film;
        BuiltFilm built;
        if (imdb_id.has_value()) {
            sources::SourceFetch f = fetch_one(http, "imdb", *imdb_id);
            imdb_film = std::move(f.film);
            built.cover_url = f.cover_url;
        }
        if (fa_id.has_value()) {
            sources::SourceFetch f = fetch_one(http, "filmaffinity", *fa_id);
            fa_film = std::move(f.film);
            built.relations = std::move(f.relations);
            built.similars = std::move(f.similars);
            if (built.cover_url.empty()) {
                built.cover_url = f.cover_url;
            }
        }
        if (bom_id.has_value()) {
            sources::SourceFetch f = fetch_one(http, "boxofficemojo", *bom_id);
            bom_film = std::move(f.film);
        }
        built.film = app::merge_films(imdb_film, fa_film, bom_film);
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

    if (args.financials && !args.imdb_id.has_value()) {
        std::cerr << "error: --financials needs --imdb (BoxOfficeMojo uses IMDb ids)\n";
        return kUsageError;
    }

    // A network client is needed for fetching and for downloading a cover.
    net::CprHttpClient http;

    // Build the film either by fetching from source(s) or from manual flags.
    Film film;
    BuiltFilm built;
    if (fetch_path) {
        const std::optional<std::string> bom_id =
            args.financials ? args.imdb_id : std::nullopt;
        int exit_code = kOk;
        auto result = build_from_sources(http, args.imdb_id, args.fa_id, bom_id, exit_code);
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
        if (args.cover && !app::fetch_and_store_cover(*repo, http, id, built.cover_url)) {
            std::cerr << "note: could not fetch a cover for film #" << id << '\n';
        }
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
    obj["has_cover"] = repo.has_cover(f.id);
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
        if (!args.fields.empty() || !args.preset.empty()) {
            try {
                const io::FieldSet sel = io::resolve_selection(
                    args.preset, args.fields, load_config(opts), io::FieldSet::all());
                write_csv(std::cout, results, sel);
            } catch (const io::FieldError& e) {
                std::cerr << "error: " << e.what() << '\n';
                return kUsageError;
            } catch (const std::exception& e) {
                std::cerr << "error: " << e.what() << '\n';
                return kRuntimeError;
            }
        } else {
            write_csv(std::cout, results);
        }
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
    const auto bom_id = ref_for(existing, "boxofficemojo");
    if (!imdb_id.has_value() && !fa_id.has_value() && !bom_id.has_value()) {
        std::cerr << "note: film #" << existing.id
                  << " has no online sources; skipping\n";
        return kOk;
    }

    int exit_code = kOk;
    auto result = build_from_sources(http, imdb_id, fa_id, bom_id, exit_code);
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

namespace {

std::string join_tokens(const std::vector<std::string>& tokens) {
    std::string out;
    for (const auto& t : tokens) {
        if (!out.empty()) {
            out += ", ";
        }
        out += t;
    }
    return out;
}

}  // namespace

int cmd_import(const GlobalOptions& opts, const ImportArgs& args) {
    if (args.emdb.empty()) {
        std::cerr << "error: provide a source file, e.g. --emdb <path>\n";
        return kUsageError;
    }

    io::FieldSet selection;
    try {
        selection = io::resolve_selection(args.preset, args.fields, load_config(opts),
                                          io::FieldSet::all());
    } catch (const io::FieldError& e) {
        std::cerr << "error: " << e.what() << '\n';
        return kUsageError;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return kRuntimeError;
    }

    auto repo = open_repo(opts);
    if (!repo) {
        return kRuntimeError;
    }
    try {
        const app::ImportStats stats =
            app::import_emdb(*repo, args.emdb, selection, args.dry_run);
        if (opts.json) {
            nlohmann::json j{{"total", stats.total},
                             {"added", stats.added},
                             {"updated", stats.updated},
                             {"dry_run", args.dry_run}};
            std::cout << j.dump(2) << '\n';
        } else {
            std::cout << (args.dry_run ? "Would import " : "Imported ") << stats.total
                      << " film(s): " << stats.added << " added, " << stats.updated
                      << " updated";
            if (args.dry_run) {
                std::cout << " (dry run, nothing written)";
            }
            std::cout << ".\n";
        }
        return kOk;
    } catch (const std::exception& e) {
        std::cerr << "error: import failed: " << e.what() << '\n';
        return kRuntimeError;
    }
}

int cmd_preset(const GlobalOptions& opts, const PresetArgs& args) {
    config::Config cfg;
    try {
        cfg = load_config(opts);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return kRuntimeError;
    }

    switch (args.action) {
        case PresetArgs::Action::List: {
            if (opts.json) {
                nlohmann::json user = nlohmann::json::object();
                for (const auto& [name, tokens] : cfg.presets()) {
                    user[name] = tokens;
                }
                nlohmann::json j{{"builtin", {"all", "userdata", "metadata"}},
                                 {"user", std::move(user)}};
                std::cout << j.dump(2) << '\n';
            } else {
                std::cout << "Built-in presets: all, userdata, metadata\n";
                if (cfg.presets().empty()) {
                    std::cout << "No user presets.\n";
                } else {
                    std::cout << "User presets:\n";
                    for (const auto& [name, tokens] : cfg.presets()) {
                        std::cout << "  " << name << ": " << join_tokens(tokens) << '\n';
                    }
                }
            }
            return kOk;
        }
        case PresetArgs::Action::Show: {
            std::optional<std::vector<std::string>> tokens;
            if (auto builtin = io::builtin_preset(args.name)) {
                tokens = builtin->tokens();
            } else {
                tokens = cfg.preset(args.name);
            }
            if (!tokens) {
                std::cerr << "error: no preset '" << args.name << "'\n";
                return kNotFound;
            }
            if (opts.json) {
                std::cout << nlohmann::json(*tokens).dump(2) << '\n';
            } else {
                std::cout << args.name << ": " << join_tokens(*tokens) << '\n';
            }
            return kOk;
        }
        case PresetArgs::Action::Set: {
            if (io::is_builtin_preset(args.name)) {
                std::cerr << "error: '" << args.name << "' is a built-in preset\n";
                return kUsageError;
            }
            io::FieldSet sel;
            try {
                sel = io::parse_field_list(args.fields);
            } catch (const io::FieldError& e) {
                std::cerr << "error: " << e.what() << '\n';
                return kUsageError;
            }
            cfg.set_preset(args.name, sel.tokens());
            try {
                cfg.save();
            } catch (const std::exception& e) {
                std::cerr << "error: " << e.what() << '\n';
                return kRuntimeError;
            }
            std::cout << "Saved preset '" << args.name << "' (" << join_tokens(sel.tokens())
                      << ").\n";
            return kOk;
        }
        case PresetArgs::Action::Remove: {
            if (io::is_builtin_preset(args.name)) {
                std::cerr << "error: cannot remove built-in preset '" << args.name << "'\n";
                return kUsageError;
            }
            if (!cfg.remove_preset(args.name)) {
                std::cerr << "error: no preset '" << args.name << "'\n";
                return kNotFound;
            }
            try {
                cfg.save();
            } catch (const std::exception& e) {
                std::cerr << "error: " << e.what() << '\n';
                return kRuntimeError;
            }
            std::cout << "Removed preset '" << args.name << "'.\n";
            return kOk;
        }
    }
    return kOk;
}

int cmd_scan(const GlobalOptions& opts, const ScanArgs& args) {
    if (args.id == kInvalidId) {
        std::cerr << "error: provide a film id to scan\n";
        return kUsageError;
    }
    auto repo = open_repo(opts);
    if (!repo) {
        return kRuntimeError;
    }
    auto film = repo->find(args.id);
    if (!film) {
        std::cerr << "error: no film with id " << args.id << '\n';
        return kNotFound;
    }

    std::string path = args.file;
    if (path.empty() && film->video.has_value()) {
        path = film->video->path;
    }
    if (path.empty()) {
        std::cerr << "error: no file to scan; pass --file or set a video path first\n";
        return kUsageError;
    }

    try {
        film->video = media::probe(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return kRuntimeError;
    }
    if (!repo->update(*film)) {
        std::cerr << "error: failed to update film #" << args.id << '\n';
        return kRuntimeError;
    }

    const auto stored = repo->find(args.id);
    if (opts.json && stored) {
        std::cout << to_json(*stored).dump(2) << '\n';
    } else if (stored && stored->video.has_value()) {
        const auto& v = *stored->video;
        std::cout << "Scanned film #" << args.id << ": " << (v.width ? *v.width : 0) << 'x'
                  << (v.height ? *v.height : 0) << ", " << v.audio_tracks.size()
                  << " audio + " << v.subtitle_tracks.size() << " subtitle track(s).\n";
    }
    return kOk;
}

int cmd_cover(const GlobalOptions& opts, const CoverArgs& args) {
    if (args.id == kInvalidId) {
        std::cerr << "error: provide a film id\n";
        return kUsageError;
    }
    auto repo = open_repo(opts);
    if (!repo) {
        return kRuntimeError;
    }
    if (!repo->find(args.id)) {
        std::cerr << "error: no film with id " << args.id << '\n';
        return kNotFound;
    }

    if (!args.set.empty()) {
        std::ifstream in(args.set, std::ios::binary);
        if (!in) {
            std::cerr << "error: could not read '" << args.set << "'\n";
            return kRuntimeError;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        const std::string bytes = ss.str();
        repo->set_cover(args.id, app::mime_from_url(args.set), bytes);
        std::cout << "Set cover for film #" << args.id << " (" << bytes.size()
                  << " bytes).\n";
        return kOk;
    }

    auto cover = repo->get_cover(args.id);
    if (!cover) {
        std::cerr << "error: film #" << args.id << " has no cover\n";
        return kNotFound;
    }
    if (!args.out.empty()) {
        std::ofstream out(args.out, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "error: could not write '" << args.out << "'\n";
            return kRuntimeError;
        }
        out.write(cover->bytes.data(), static_cast<std::streamsize>(cover->bytes.size()));
        std::cout << "Wrote " << cover->bytes.size() << " bytes to " << args.out << ".\n";
        return kOk;
    }

    if (opts.json) {
        std::cout << nlohmann::json{{"id", args.id},
                                    {"mime", cover->mime},
                                    {"size_bytes", cover->bytes.size()}}
                         .dump(2)
                  << '\n';
    } else {
        std::cout << "Film #" << args.id << " has a " << cover->mime << " cover ("
                  << cover->bytes.size() << " bytes). Use --out to export it.\n";
    }
    return kOk;
}

int cmd_play(const GlobalOptions& opts, Id id) {
    auto repo = open_repo(opts);
    if (!repo) {
        return kRuntimeError;
    }
    auto film = repo->find(id);
    if (!film) {
        std::cerr << "error: no film with id " << id << '\n';
        return kNotFound;
    }
    if (!film->video.has_value() || film->video->path.empty()) {
        std::cerr << "error: film #" << id << " has no video file path\n";
        return kUsageError;
    }
    if (!app::open_in_default_app(film->video->path)) {
        std::cerr << "error: could not launch a player for '" << film->video->path
                  << "'\n";
        return kRuntimeError;
    }
    std::cout << "Opening " << film->video->path << " ...\n";
    return kOk;
}

}  // namespace pfdb::cli
