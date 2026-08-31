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

    // --- init ---
    auto* init = app.add_subcommand("init", "Create or upgrade the database file");

    // --- add ---
    AddArgs add_args;
    auto* add = app.add_subcommand("add", "Add a film to the collection manually");
    add->add_option("--title", add_args.title, "Film title")->required();
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

    // --- list ---
    auto* list = app.add_subcommand("list", "List films in the collection");

    // --- remove ---
    pfdb::Id remove_id = pfdb::kInvalidId;
    auto* remove = app.add_subcommand("remove", "Remove a film by id");
    remove->add_option("id", remove_id, "Film id to remove")->required();

    // Let global options given after the subcommand fall through to the parent.
    for (auto* sub : std::array{init, add, list, remove}) {
        sub->fallthrough();
    }

    CLI11_PARSE(app, argc, argv);

    if (init->parsed()) {
        return cmd_init(gopts);
    }
    if (add->parsed()) {
        return cmd_add(gopts, add_args);
    }
    if (list->parsed()) {
        return cmd_list(gopts);
    }
    if (remove->parsed()) {
        return cmd_remove(gopts, remove_id);
    }
    return kUsageError;  // require_subcommand(1) makes this unreachable
}
