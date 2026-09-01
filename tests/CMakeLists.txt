find_package(Catch2 3 CONFIG REQUIRED)

add_executable(pfdb_tests
    unit/test_domain.cpp
    unit/test_repository.cpp
    unit/test_collection_model.cpp
    unit/test_json.cpp
    unit/test_imdb_parser.cpp
    unit/test_imdb_source.cpp
    unit/test_imdb_live.cpp
)

# Tests reach into the internal headers under src/ (Repository, CollectionModel,
# sources) and this directory (support/ helpers).
target_include_directories(pfdb_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Location of the on-disk test fixtures, read by tests/support/fixtures.hpp.
target_compile_definitions(pfdb_tests PRIVATE
    PFDB_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_link_libraries(pfdb_tests
    PRIVATE
        pfdb_core
        Catch2::Catch2WithMain
        nlohmann_json::nlohmann_json
        pfdb_warnings
)

# Register each TEST_CASE with CTest.
include(Catch)
catch_discover_tests(pfdb_tests)
