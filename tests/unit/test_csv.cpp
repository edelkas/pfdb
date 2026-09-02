#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "io/csv.hpp"
#include "pfdb/film.hpp"

using namespace pfdb;

namespace {

std::string csv_of(const std::vector<const Film*>& films) {
    std::ostringstream os;
    write_csv(os, films);
    return os.str();
}

}  // namespace

TEST_CASE("csv writes a header and one row per film", "[csv]") {
    Film f;
    f.id = 1;
    f.title = "Blade Runner";
    f.original_title = "Blade Runner";
    f.year = 1982;
    f.runtime_minutes = 117;
    f.genres = {"Sci-Fi", "Action"};
    f.ratings.push_back({"imdb", 8.1, 10.0, std::int64_t{800000}});
    f.user.date_watched = "2024-05-01";
    f.user.favorite = true;

    const std::string out = csv_of({&f});
    REQUIRE(out.rfind("id,title,original_title,year,runtime,genres,imdb_rating,fa_rating,"
                      "date_watched,favorite,my_rating\r\n",
                      0) == 0);
    REQUIRE(out.find("1,Blade Runner,Blade Runner,1982,117,Sci-Fi; Action,8.1,,"
                     "2024-05-01,true,\r\n") != std::string::npos);
}

TEST_CASE("csv quotes fields containing commas and quotes", "[csv]") {
    Film f;
    f.id = 2;
    f.title = R"(Hello, "World")";  // has a comma and quotes

    const std::string out = csv_of({&f});
    // Comma + embedded quotes -> whole field quoted, inner quotes doubled.
    REQUIRE(out.find(R"("Hello, ""World""")") != std::string::npos);
}
