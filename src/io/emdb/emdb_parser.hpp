#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pfdb::io::emdb {

/// Thrown when the EMDB file cannot be parsed.
class EmdbError : public std::runtime_error {
public:
    explicit EmdbError(const std::string& message) : std::runtime_error(message) {}
};

/// One movie: each field group (title/year/genres/...) split on 0x1E and
/// un-tagged. A missing group maps to an empty vector.
struct EmdbMovie {
    std::map<std::string, std::vector<std::string>> groups;

    /// Field `index` of `group`, or "" if the group or index is absent.
    std::string field(std::string_view group, std::size_t index) const;
};

/// A person entry (directors/writers/composers carry only a name).
struct EmdbPerson {
    std::string name;
    std::string id;  // IMDb/TMDB id; empty for crew arrays
};

/// The whole decoded database, with the index-based lookup tables resolved.
struct EmdbData {
    int version = 0;
    std::vector<EmdbPerson> actors;
    std::vector<EmdbPerson> directors;
    std::vector<EmdbPerson> writers;
    std::vector<EmdbPerson> composers;
    std::vector<std::string> tags;                // tag name by index
    std::map<std::string, std::string> collections;  // collection id -> name
    std::map<std::string, std::string> custom_genres;  // slot "00".."09" -> name
    std::vector<EmdbMovie> movies;
};

/// Parse the sanitized JSON produced by bytes_to_json(). Throws EmdbError.
EmdbData parse(const std::string& json);

}  // namespace pfdb::io::emdb
