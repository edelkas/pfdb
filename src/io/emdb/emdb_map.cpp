#include "io/emdb/emdb_map.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace pfdb::io::emdb {
namespace {

// --- small parsing helpers ---

std::optional<long long> to_int(const std::string& s) {
    if (s.empty()) {
        return std::nullopt;
    }
    try {
        std::size_t pos = 0;
        const long long v = std::stoll(s, &pos, 10);
        return pos == s.size() ? std::optional<long long>(v) : std::nullopt;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<long long> to_hex(const std::string& s) {
    if (s.empty()) {
        return std::nullopt;
    }
    try {
        return std::stoll(s, nullptr, 16);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!s.empty()) {
        out.push_back(cur);
    }
    return out;
}

std::vector<int> parse_indexes(const std::string& csv) {
    std::vector<int> out;
    for (const auto& part : split(csv, ',')) {
        if (auto v = to_int(part)) {
            out.push_back(static_cast<int>(*v));
        }
    }
    return out;
}

/// "YYYYMMDD" -> "YYYY-MM-DD", or nullopt if not a real date.
std::optional<std::string> to_iso_date(const std::string& s) {
    if (s.size() != 8) {
        return std::nullopt;
    }
    for (char c : s) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
    }
    if (s == "00000000") {
        return std::nullopt;
    }
    return s.substr(0, 4) + "-" + s.substr(4, 2) + "-" + s.substr(6, 2);
}

std::string_view genre_name(char code) {
    switch (code) {
        case 'A': return "Action";
        case 'X': return "Adult";
        case 'V': return "Adventure";
        case 'C': return "Animation";
        case '@': return "Biography";
        case 'K': return "Comedy";
        case '!': return "Crime";
        case 'U': return "Documentary";
        case 'D': return "Drama";
        case 'I': return "Family";
        case 'F': return "Fantasy";
        case 'f': return "Film-noir";
        case 'g': return "Game show";
        case 'G': return "History";
        case 'H': return "Horror";
        case 'M': return "Music";
        case 'm': return "Musical";
        case 'Y': return "Mystery";
        case 'N': return "News";
        case 'r': return "Reality TV";
        case 'R': return "Romance";
        case 'S': return "Sci-Fi";
        case 's': return "Short";
        case 'P': return "Sport";
        case 't': return "Talk show";
        case 'T': return "Thriller";
        case 'O': return "War";
        case 'W': return "Western";
        case '#': return "TV series";
        default: return {};
    }
}

// Custom genre slot for the digit codes 1..9,0 -> "00".."09".
std::optional<std::string> custom_genre_slot(char code) {
    if (code >= '1' && code <= '9') {
        return std::string("0") + static_cast<char>('0' + (code - '1'));
    }
    if (code == '0') {
        return "09";
    }
    return std::nullopt;
}

std::vector<std::string> decode_genres(const std::string& codes, const EmdbData& data) {
    std::vector<std::string> out;
    for (char c : codes) {
        if (const auto name = genre_name(c); !name.empty()) {
            out.emplace_back(name);
        } else if (const auto slot = custom_genre_slot(c)) {
            if (auto it = data.custom_genres.find(*slot); it != data.custom_genres.end()) {
                out.push_back(it->second);
            }
        }
    }
    return out;
}

void add_credits(std::vector<Credit>& credits, const std::vector<int>& idx,
                 const std::vector<EmdbPerson>& people, CreditRole role,
                 const std::vector<std::string>& characters = {}) {
    int order = 0;
    for (int i : idx) {
        if (i < 0 || static_cast<std::size_t>(i) >= people.size()) {
            ++order;
            continue;
        }
        Credit c;
        c.person.name = people[static_cast<std::size_t>(i)].name;
        c.role = role;
        if (static_cast<std::size_t>(order) < characters.size()) {
            c.character = characters[static_cast<std::size_t>(order)];
        }
        c.order = order;
        credits.push_back(std::move(c));
        ++order;
    }
}

// Resolution enum -> (width, height) for the entries with explicit dimensions.
std::optional<std::pair<int, int>> resolution_dims(const std::string& res) {
    if (res.empty()) {
        return std::nullopt;
    }
    // Enum form: "@N".
    if (res[0] == '@') {
        static const std::pair<int, int> kDims[] = {
            {720, 576}, {720, 480}, {1280, 720}, {1920, 1080}};
        if (auto n = to_int(res.substr(1))) {
            if (*n >= 0 && *n < 4) {
                return kDims[*n];
            }
            if (*n == 8) {
                return std::pair<int, int>{3840, 2160};
            }
        }
        return std::nullopt;
    }
    // Literal "WxH".
    if (const auto pos = res.find('x'); pos != std::string::npos) {
        const auto w = to_int(res.substr(0, pos));
        const auto h = to_int(res.substr(pos + 1));
        if (w && h) {
            return std::pair<int, int>{static_cast<int>(*w), static_cast<int>(*h)};
        }
    }
    return std::nullopt;
}

std::string_view codec_name(long long codec_id) {
    switch (codec_id) {
        case 1: return "MPEG-2";
        case 2: return "XviD";
        case 3: return "DivX";
        case 4: return "WMV";
        case 6: return "H.264";
        case 9: return "H.264";
        case 11: return "MPEG-4";
        case 13: return "H.265";
        case 15: return "VP8";
        case 16: return "VP9";
        case 17: return "AV1";
        default: return {};
    }
}

}  // namespace

std::vector<Film> to_films(const EmdbData& data, const FieldSet& selection) {
    const auto sel = [&](Field f) { return selection.contains(f); };
    std::vector<Film> films;
    films.reserve(data.movies.size());

    for (const auto& m : data.movies) {
        Film f;

        if (sel(Field::Title)) {
            f.title = m.field("title", 0);
        }
        if (sel(Field::OriginalTitle)) {
            f.original_title = m.field("title", 1);  // "also known as" (best-effort)
        }
        if (sel(Field::Year)) {
            if (auto y = to_int(m.field("year", 0)); y && *y > 0) {
                f.year = static_cast<int>(*y);
            }
        }
        if (sel(Field::Runtime)) {
            if (auto r = to_int(m.field("year", 2)); r && *r > 0) {
                f.runtime_minutes = static_cast<int>(*r);
            }
        }
        if (sel(Field::Synopsis)) {
            f.synopsis = m.field("plot", 0);
        }
        if (sel(Field::Genres)) {
            f.genres = decode_genres(m.field("genres", 0), data);
        }
        if (sel(Field::Topics)) {
            for (int i : parse_indexes(m.field("tagline", 1))) {
                if (i >= 0 && static_cast<std::size_t>(i) < data.tags.size()) {
                    f.topics.push_back(data.tags[static_cast<std::size_t>(i)]);
                }
            }
        }
        if (sel(Field::Groups)) {
            // Collection id is the last title field (the older "unknown" field was
            // dropped circa v70, so it sits at index 3 in current files).
            const std::string cid = m.field("title", 3);
            if (!cid.empty() && cid != "-1") {
                if (auto it = data.collections.find(cid); it != data.collections.end()) {
                    f.groups.push_back(it->second);
                }
            }
        }
        if (sel(Field::Cast)) {
            add_credits(f.credits, parse_indexes(m.field("cast", 0)), data.actors,
                        CreditRole::Actor, split(m.field("cast", 1), '|'));
        }
        if (sel(Field::Directors)) {
            add_credits(f.credits, parse_indexes(m.field("year", 1)), data.directors,
                        CreditRole::Director);
        }
        if (sel(Field::Writers)) {
            add_credits(f.credits, parse_indexes(m.field("year", 15)), data.writers,
                        CreditRole::Writer);
        }
        if (sel(Field::Composers)) {
            add_credits(f.credits, parse_indexes(m.field("year", 16)), data.composers,
                        CreditRole::Composer);
        }

        // Identity anchor + imdb rating.
        std::string imdb_id = split(m.field("genres", 1), '|').empty()
                                  ? std::string()
                                  : split(m.field("genres", 1), '|')[0];
        if (sel(Field::ImdbId) && !imdb_id.empty() && imdb_id != "0") {
            f.source_refs.push_back({"imdb", "tt" + imdb_id, std::nullopt});
        }
        if (sel(Field::ImdbRating)) {
            if (auto r = to_hex(m.field("rating", 0)); r && *r > 0) {
                Rating rating;
                rating.source = "imdb";
                rating.value = static_cast<double>(*r) / 10.0;
                rating.scale = 10.0;
                if (auto votes = to_int(m.field("rating", 7))) {
                    rating.votes = *votes;
                }
                f.ratings.push_back(std::move(rating));
            }
        }

        // --- user data ---
        if (sel(Field::UserRating)) {
            if (auto r = to_hex(m.field("rating", 1)); r && *r > 0) {
                f.user.personal_rating = static_cast<double>(*r) / 10.0;
            }
        }
        if (sel(Field::WatchDate)) {
            f.user.date_watched = to_iso_date(m.field("genres", 8));
        }
        const auto personal = to_int(m.field("year", 9));
        const long long packed = personal.value_or(0);
        if (sel(Field::WatchCount)) {
            f.user.watch_count = static_cast<int>(packed >> 8);
        }
        if (sel(Field::Owned)) {
            f.user.owned = ((packed >> 2) & 1) != 0;
        }
        if (sel(Field::Wishlist)) {
            f.user.wishlist = ((packed >> 1) & 1) != 0;
        }
        if (sel(Field::Favorite)) {
            f.user.favorite = ((packed >> 5) & 1) != 0;
        }
        if (sel(Field::Comments)) {
            f.user.notes = m.field("comments", 0);
        }

        // --- video file ---
        if (sel(Field::VideoFile)) {
            const std::string path = m.field("year", 7);
            const auto size_mb = to_int(m.field("rating", 8));
            const auto codec_packed = to_int(m.field("year", 4));
            const auto dims = resolution_dims(m.field("year", 6));
            if (!path.empty() || size_mb || dims) {
                VideoFileInfo v;
                v.path = path;
                if (size_mb && *size_mb > 0) {
                    v.size_bytes = *size_mb * 1024 * 1024;
                }
                if (codec_packed) {
                    v.codec = std::string(codec_name(*codec_packed % 100));
                }
                if (dims) {
                    v.width = dims->first;
                    v.height = dims->second;
                }
                f.video = std::move(v);
            }
        }

        films.push_back(std::move(f));
    }

    return films;
}

}  // namespace pfdb::io::emdb
