#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>

#include "parse/html.hpp"

using namespace pfdb::parse;

TEST_CASE("html wrapper: select, text and attributes", "[html]") {
    HtmlDocument doc(R"(<html><body>
        <div id="main"><span itemprop="name">Blade Runner</span></div>
        <a class="t" href="/movietopic.php?topic=1">Neo-noir</a>
        <a class="t" href="/movietopic.php?topic=2">Thriller futurista</a>
        <meta itemprop="ratingValue" content="8.1">
      </body></html>)");

    auto name = doc.select_first("#main [itemprop=name]");
    REQUIRE(name.has_value());
    REQUIRE(name->text() == "Blade Runner");

    const auto topics = doc.select(R"(a[href*="movietopic.php"])");
    REQUIRE(topics.size() == 2);
    REQUIRE(topics[0].text() == "Neo-noir");
    REQUIRE(topics[1].text() == "Thriller futurista");

    auto rating = doc.select_first("[itemprop=ratingValue]");
    REQUIRE(rating.has_value());
    REQUIRE(rating->attr("content") == std::optional<std::string>("8.1"));
    REQUIRE_FALSE(rating->attr("missing").has_value());
}

TEST_CASE("html wrapper: scoped select and no-match", "[html]") {
    HtmlDocument doc(R"(<html><body>
        <div class="card"><span class="k">A</span></div>
        <div class="card"><span class="k">B</span></div>
      </body></html>)");

    const auto cards = doc.select("div.card");
    REQUIRE(cards.size() == 2);
    REQUIRE(cards[1].select_first("span.k")->text() == "B");
    REQUIRE(doc.select("nope").empty());
    REQUIRE_FALSE(doc.select_first("nope").has_value());
}
