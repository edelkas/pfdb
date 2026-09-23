#include <array>
#include <string>

#include <CLI/CLI.hpp>

#include "cli/commands.hpp"

// PFDB_VERSION is injected by CMake (see src/CMakeLists.txt).
#ifndef PFDB_VERSION
#define PFDB_VERSION "0.0.0-dev"
#endif

int main(int argc, char** argv) {
    using namespace pfdb::cli;

    CLI::App app{"PFDB - Power Film DataBase: a power-user film collection manager",
                 "pfdb"};
    app.require_subcommand(1);
    app.set_version_flag("--version", std::string("pfdb ") + PFDB_VERSION);

    // --- Global options (may appear before or after the subcommand). ---
    GlobalOptions gopts;
    gopts.db_path = "pfdb.db";
    app.add_option("--db", gopts.db_path, "Path to the collection database file")
        ->envname("PFDB_DATABASE")
        ->capture_default_str();
    app.add_flag("--json", gopts.json, "Emit machine-readable JSON on stdout");
    app.add_option("--config", gopts.config_path,
                   "Path to the user config (field presets)")
        ->envname("PFDB_CONFIG");

    // --- init ---
    auto* init = app.add_subcommand("init", "Create or upgrade the database file");

    // --- add ---
    // Either fetch from a source (--imdb <id>) or add manually (--title ...).
    AddArgs add_args;
    auto* add = app.add_subcommand(
        "add", "Add a film: fetch from IMDb (--imdb) or enter manually (--title)");
    add->add_option("--imdb", add_args.imdb_id, "Fetch from IMDb by title id, e.g. tt0083658");
    add->add_option("--fa", add_args.fa_id,
                    "Fetch from FilmAffinity by id, e.g. 358476 (combine with --imdb)");
    add->add_option("--title", add_args.title,
                    "Film title (required for a manual add; overrides fetched title)");
    add->add_option("--original-title", add_args.original_title,
                    "Original-language title");
    add->add_option("--year", add_args.year, "Release year");
    add->add_option("--runtime", add_args.runtime_minutes, "Runtime in minutes");
    add->add_option("--synopsis", add_args.synopsis, "Plot synopsis");
    add->add_option("--genre", add_args.genres, "Genre (repeat for multiple)");
    add->add_option("--date-watched", add_args.date_watched,
                    "Date watched, YYYY-MM-DD");
    add->add_option("--rating", add_args.personal_rating,
                    "Your personal rating (0-10)");
    add->add_option("--notes", add_args.notes, "Free-form notes");
    add->add_flag("--favorite", add_args.favorite, "Mark as a favourite");
    add->add_flag("--dry-run", add_args.dry_run,
                  "Fetch/build the film and print it, but do not save");

    // --- search ---
    SearchArgs search_args;
    auto* search = app.add_subcommand("search", "Search a source for films");
    search->add_option("query", search_args.query, "Search text")->required();
    search->add_option("--source", search_args.source, "Source to search")
        ->capture_default_str();

    // --- update ---
    UpdateArgs update_args;
    auto* update = app.add_subcommand(
        "update", "Re-fetch a film's sources to refresh scores/relations (keeps your data)");
    update->add_option("id", update_args.id, "Film id to update");
    update->add_flag("--all", update_args.all, "Update every film in the collection");

    // --- list ---
    ListArgs list_args;
    auto* list = app.add_subcommand("list", "List films, with optional filters/sort");
    list->add_option("--filter,-f", list_args.filters,
                     "A filter 'FIELD OP VALUE' (repeatable); referenced as F1, F2, ...");
    list->add_option("--where,-w", list_args.where,
                     "Boolean expression over the filters, e.g. 'F1 AND (F2 OR F3)'");
    list->add_option("--sort,-s", list_args.sort,
                     "Sort spec 'field[:asc|desc],...', e.g. 'year:desc,title'");
    list->add_flag("--csv", list_args.csv, "Emit CSV to stdout (excludes --json)");
    list->add_option("--fields", list_args.fields,
                     "CSV of field tokens to export (with --csv)");
    list->add_option("--preset", list_args.preset,
                     "Named field preset to export (with --csv)");

    // --- import ---
    ImportArgs import_args;
    auto* import = app.add_subcommand("import", "Import a collection (e.g. from EMDB)");
    import->add_option("--emdb", import_args.emdb, "Path to an EMDB emdb.dat file");
    import->add_option("--preset", import_args.preset,
                       "Field preset to import (default: all)");
    import->add_option("--fields", import_args.fields,
                       "CSV of field tokens to import (overrides --preset)");
    import->add_flag("--dry-run", import_args.dry_run,
                     "Parse and report, but do not write to the database");

    // --- preset ---
    PresetArgs preset_args;
    auto* preset = app.add_subcommand("preset", "Manage import/export field presets");
    preset->require_subcommand(1);
    preset->add_subcommand("list", "List presets");
    auto* preset_show = preset->add_subcommand("show", "Show a preset's fields");
    preset_show->add_option("name", preset_args.name, "Preset name")->required();
    auto* preset_set = preset->add_subcommand("set", "Define or replace a user preset");
    preset_set->add_option("name", preset_args.name, "Preset name")->required();
    preset_set->add_option("fields", preset_args.fields,
                           "CSV of field tokens")
        ->required();
    auto* preset_remove = preset->add_subcommand("remove", "Remove a user preset");
    preset_remove->add_option("name", preset_args.name, "Preset name")->required();

    // --- remove ---
    pfdb::Id remove_id = pfdb::kInvalidId;
    auto* remove = app.add_subcommand("remove", "Remove a film by id");
    remove->add_option("id", remove_id, "Film id to remove")->required();

    // Let global options given after the subcommand fall through to the parent.
    for (auto* sub : std::array{init, add, search, update, list, import, remove}) {
        sub->fallthrough();
    }

    CLI11_PARSE(app, argc, argv);

    if (init->parsed()) {
        return cmd_init(gopts);
    }
    if (add->parsed()) {
        return cmd_add(gopts, add_args);
    }
    if (search->parsed()) {
        return cmd_search(gopts, search_args);
    }
    if (update->parsed()) {
        return cmd_update(gopts, update_args);
    }
    if (list->parsed()) {
        return cmd_list(gopts, list_args);
    }
    if (import->parsed()) {
        return cmd_import(gopts, import_args);
    }
    if (preset->parsed()) {
        if (preset_show->parsed()) {
            preset_args.action = PresetArgs::Action::Show;
        } else if (preset_set->parsed()) {
            preset_args.action = PresetArgs::Action::Set;
        } else if (preset_remove->parsed()) {
            preset_args.action = PresetArgs::Action::Remove;
        } else {
            preset_args.action = PresetArgs::Action::List;
        }
        return cmd_preset(gopts, preset_args);
    }
    if (remove->parsed()) {
        return cmd_remove(gopts, remove_id);
    }
    return kUsageError;  // require_subcommand(1) makes this unreachable
}
