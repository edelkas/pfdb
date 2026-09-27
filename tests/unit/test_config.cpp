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

TEST_CASE("config persists and reloads update settings", "[config]") {
    const std::string path = temp_config_path();
    std::filesystem::remove(path);

    Config cfg = Config::load(path);
    REQUIRE(cfg.update().check_interval_hours == 24);  // default
    REQUIRE(cfg.update().last_check == 0);

    cfg.update().check_interval_hours = 12;
    cfg.update().last_check = 1'700'000'000;
    cfg.update().last_run_version = "0.1.0";
    cfg.update().pending_version = "0.2.0";
    cfg.update().pending_notes = "Notes here";
    cfg.update().repo = "someone/pfdb";
    cfg.save();

    const Config reloaded = Config::load(path);
    REQUIRE(reloaded.update().check_interval_hours == 12);
    REQUIRE(reloaded.update().last_check == 1'700'000'000);
    REQUIRE(reloaded.update().last_run_version == "0.1.0");
    REQUIRE(reloaded.update().pending_version == "0.2.0");
    REQUIRE(reloaded.update().pending_notes == "Notes here");
    REQUIRE(reloaded.update().repo == "someone/pfdb");

    std::filesystem::remove(path);
}

TEST_CASE("update_check_due respects the interval", "[config]") {
    Config cfg;  // defaults: 24h, never checked
    REQUIRE(cfg.update_check_due(1'000'000));  // never checked -> due

    cfg.mark_update_checked(1'000'000);
    REQUIRE_FALSE(cfg.update_check_due(1'000'000 + 3600));          // 1h later
    REQUIRE(cfg.update_check_due(1'000'000 + 25 * 3600));           // 25h later

    cfg.update().check_interval_hours = 0;  // disabled
    REQUIRE_FALSE(cfg.update_check_due(1'000'000 + 10'000'000));
}
