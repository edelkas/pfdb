#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "app/config.hpp"

using namespace pfdb::config;

namespace {

std::string temp_config_path() {
    return (std::filesystem::temp_directory_path() / "pfdb_test_config.json").string();
}

}  // namespace

TEST_CASE("config persists and reloads user presets", "[config]") {
    const std::string path = temp_config_path();
    std::filesystem::remove(path);

    Config cfg = Config::load(path);  // missing file -> empty
    REQUIRE(cfg.presets().empty());

    cfg.set_preset("mine", {"title", "year", "user-rating"});
    cfg.set_preset("watched", {"watch-date", "watch-count"});
    cfg.save();

    const Config reloaded = Config::load(path);
    REQUIRE(reloaded.presets().size() == 2);
    REQUIRE(reloaded.preset("mine").value() ==
            std::vector<std::string>{"title", "year", "user-rating"});
    REQUIRE_FALSE(reloaded.preset("absent").has_value());

    std::filesystem::remove(path);
}

TEST_CASE("config removes presets", "[config]") {
    const std::string path = temp_config_path();
    std::filesystem::remove(path);

    Config cfg = Config::load(path);
    cfg.set_preset("a", {"title"});
    REQUIRE(cfg.remove_preset("a"));
    REQUIRE_FALSE(cfg.remove_preset("a"));  // already gone
    REQUIRE(cfg.presets().empty());

    std::filesystem::remove(path);
}

TEST_CASE("config default_path is non-empty", "[config]") {
    REQUIRE_FALSE(Config::default_path().empty());
}
