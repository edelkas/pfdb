#include "io/emdb/emdb_parser.hpp"

#include <array>
#include <utility>

#include <nlohmann/json.hpp>

namespace pfdb::io::emdb {
namespace {

/// Replace EMDB's literal-character tags with the characters they stand for.
std::string untag(std::string s) {
    static const std::array<std::pair<std::string_view, char>, 5> kTags = {{
        {"<DQ>", '"'},
        {"<SCB>", ']'},   // before <SC> so the longer match wins
        {"<SC>", '['},
        {"<CCB>", '}'},   // before <CB>
        {"<CB>", '{'},
    }};
    for (const auto& [tag, ch] : kTags) {
        std::size_t pos = 0;
        while ((pos = s.find(tag, pos)) != std::string::npos) {
            s.replace(pos, tag.size(), 1, ch);
            pos += 1;
        }
    }
    return s;
}

/// Split a group string on the 0x1E record separator and un-tag each field.
std::vector<std::string> split_fields(const std::string& value) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : value) {
        if (c == '\x1e') {
            out.push_back(untag(std::move(cur)));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(untag(std::move(cur)));
    return out;
}

std::string get_string(const nlohmann::json& obj, const char* key) {
    auto it = obj.find(key);
    if (it != obj.end() && it->is_string()) {
        return it->get<std::string>();
    }
    return {};
}

std::vector<EmdbPerson> parse_people(const nlohmann::json& j, const char* key,
                                     bool with_id) {
    std::vector<EmdbPerson> out;
    auto it = j.find(key);
    if (it == j.end() || !it->is_array()) {
        return out;
    }
    for (const auto& e : *it) {
        EmdbPerson p;
        if (e.is_string()) {
            p.name = e.get<std::string>();
        } else if (e.is_object()) {
            p.name = get_string(e, "name");
            if (with_id) {
                p.id = get_string(e, "id");
            }
        }
        out.push_back(std::move(p));
    }
    return out;
}

}  // namespace

std::string EmdbMovie::field(std::string_view group, std::size_t index) const {
    auto it = groups.find(std::string(group));
    if (it == groups.end() || index >= it->second.size()) {
        return {};
    }
    return it->second[index];
}

EmdbData parse(const std::string& json) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
    } catch (const nlohmann::json::exception& e) {
        throw EmdbError(std::string("could not parse emdb data: ") + e.what());
    }
    if (!j.is_object()) {
        throw EmdbError("emdb data is not a JSON object");
    }

    EmdbData data;
    if (auto it = j.find("version"); it != j.end() && it->is_string()) {
        try {
            data.version = std::stoi(it->get<std::string>());
        } catch (const std::exception&) {
            data.version = 0;
        }
    }

    data.actors = parse_people(j, "actors", /*with_id=*/true);
    data.directors = parse_people(j, "directors", /*with_id=*/false);
    data.writers = parse_people(j, "writers", /*with_id=*/false);
    data.composers = parse_people(j, "composers", /*with_id=*/false);

    if (auto it = j.find("tags"); it != j.end() && it->is_array()) {
        for (const auto& t : *it) {
            data.tags.push_back(get_string(t, "name"));
        }
    }
    if (auto it = j.find("collections"); it != j.end() && it->is_array()) {
        for (const auto& c : *it) {
            data.collections[get_string(c, "id")] = get_string(c, "name");
        }
    }
    if (auto it = j.find("custom-genres"); it != j.end() && it->is_object()) {
        for (const auto& [slot, name] : it->items()) {
            if (name.is_string()) {
                data.custom_genres[slot] = name.get<std::string>();
            }
        }
    }

    if (auto it = j.find("movies"); it != j.end() && it->is_array()) {
        for (const auto& m : *it) {
            if (!m.is_object()) {
                continue;
            }
            EmdbMovie movie;
            for (const auto& [group, value] : m.items()) {
                if (value.is_string()) {
                    movie.groups[group] = split_fields(value.get<std::string>());
                }
            }
            data.movies.push_back(std::move(movie));
        }
    }

    return data;
}

}  // namespace pfdb::io::emdb
