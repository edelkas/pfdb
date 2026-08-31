find_package(Catch2 3 CONFIG REQUIRED)

add_executable(pfdb_tests
    unit/test_domain.cpp
    unit/test_repository.cpp
    unit/test_collection_model.cpp
    unit/test_json.cpp
)

# Tests reach into the internal headers under src/ (Repository, CollectionModel).
target_include_directories(pfdb_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

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
