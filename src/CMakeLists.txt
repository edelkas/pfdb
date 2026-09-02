# ---------------------------------------------------------------------------
# pfdb_core: all application logic, linked by both the CLI and the tests.
# ---------------------------------------------------------------------------
add_library(pfdb_core STATIC
    domain/credit.cpp
    db/repository.cpp
    model/collection_model.cpp
    io/film_json.cpp
    io/csv.cpp
    query/field.cpp
    query/filter.cpp
    query/expr.cpp
    query/parser.cpp
    query/normalize.cpp
    query/sort.cpp
    query/engine.cpp
    net/http_client.cpp
    parse/html.cpp
    sources/imdb/imdb_parser.cpp
    sources/imdb/imdb_source.cpp
    sources/filmaffinity/fa_parser.cpp
    sources/filmaffinity/fa_source.cpp
    sources/source_registry.cpp
    app/enrichment.cpp
)

target_include_directories(pfdb_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pfdb_core
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        SQLiteCpp
        spdlog::spdlog
        cpr::cpr
        lexbor::lexbor
        pfdb_warnings
)

# Project version is made available to the code as a macro.
target_compile_definitions(pfdb_core PUBLIC PFDB_VERSION="${PROJECT_VERSION}")

# ---------------------------------------------------------------------------
# pfdb_cli: the command-line front-end.
# ---------------------------------------------------------------------------
add_executable(pfdb_cli
    cli/main.cpp
    cli/commands.cpp
)

set_target_properties(pfdb_cli PROPERTIES OUTPUT_NAME pfdb)

target_include_directories(pfdb_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pfdb_cli
    PRIVATE
        pfdb_core
        CLI11::CLI11
        spdlog::spdlog
        pfdb_warnings
)
