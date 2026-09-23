#include <catch2/catch_test_macros.hpp>

#include "io/emdb/emdb_decode.hpp"
#include "io/emdb/emdb_map.hpp"
#include "io/emdb/emdb_parser.hpp"
#include "io/field_set.hpp"
#include "support/fixtures.hpp"

using namespace pfdb;

// Hidden by default (the ~5 MB real export lives outside CI). Run explicitly with
// `pfdb_tests "[.emdb-big]"` to check the parser against a full real file.
TEST_CASE("emdb parses the full real export", "[.emdb-big]") {
    const std::string bytes = pfdb::test::read_fixture("emdb.dat");
    const auto data = io::emdb::parse(io::emdb::bytes_to_json(bytes));
    REQUIRE(data.version >= 58);
    REQUIRE(data.movies.size() > 1000);

    const auto films = io::emdb::to_films(data, io::FieldSet::all());
    REQUIRE(films.size() == data.movies.size());
    // Every film that carries an IMDb id should format it as tt<digits>.
    for (const auto& f : films) {
        for (const auto& r : f.source_refs) {
            if (r.source == "imdb") {
                REQUIRE(r.external_id.rfind("tt", 0) == 0);
            }
        }
    }
}
