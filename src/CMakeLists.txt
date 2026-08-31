# ---------------------------------------------------------------------------
# pfdb_core: all application logic, linked by both the CLI and the tests.
# ---------------------------------------------------------------------------
add_library(pfdb_core STATIC
    domain/credit.cpp
    db/repository.cpp
    model/collection_model.cpp
    io/film_json.cpp
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
