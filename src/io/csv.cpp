#include "io/csv.hpp"

#include <sstream>
#include <string>

#include "query/field.hpp"

namespace pfdb {
namespace {

/// Quote a field per RFC 4180 when it contains a comma, quote, CR, or LF.
std::string escape(const std::string& in) {
    const bool needs_quotes = in.find_first_of(",\"\r\n") != std::string::npos;
    if (!needs_quotes) {
        return in;
    }
    std::string out = "\"";
    for (const char c : in) {
        if (c == '"') {
            out += '"';  // double an embedded quote
        }
        out += c;
    }
    out += '"';
    return out;
}

std::string join_genres(const Film& f) {
    std::string out;
    for (std::size_t i = 0; i < f.genres.size(); ++i) {
        if (i != 0) {
            out += "; ";
        }
        out += f.genres[i];
    }
    return out;
}

std::string opt_int(const std::optional<int>& v) {
    return v.has_value() ? std::to_string(*v) : std::string{};
}

std::string opt_double(const std::optional<double>& v) {
    if (!v.has_value()) {
        return {};
    }
    std::ostringstream ss;
    ss << *v;
    return ss.str();
}

}  // namespace

void write_csv(std::ostream& os, const std::vector<const Film*>& films) {
    os << "id,title,original_title,year,runtime,genres,imdb_rating,fa_rating,"
          "date_watched,favorite,my_rating\r\n";
    for (const Film* f : films) {
        os << f->id << ',' << escape(f->title) << ',' << escape(f->original_title) << ','
           << opt_int(f->year) << ',' << opt_int(f->runtime_minutes) << ','
           << escape(join_genres(*f)) << ','
           << opt_double(query::source_rating(*f, "imdb")) << ','
           << opt_double(query::source_rating(*f, "filmaffinity")) << ','
           << escape(f->user.date_watched.value_or("")) << ','
           << (f->user.favorite ? "true" : "false") << ','
           << opt_double(f->user.personal_rating) << "\r\n";
    }
}

namespace {

std::string join_names(const Film& f, CreditRole role) {
    std::string out;
    for (const auto& c : f.credits) {
        if (c.role == role) {
            if (!out.empty()) {
                out += "; ";
            }
            out += c.person.name;
        }
    }
    return out;
}

std::string join_list(const std::vector<std::string>& xs) {
    std::string out;
    for (const auto& x : xs) {
        if (!out.empty()) {
            out += "; ";
        }
        out += x;
    }
    return out;
}

std::string imdb_ext_id(const Film& f) {
    for (const auto& r : f.source_refs) {
        if (r.source == "imdb") {
            return r.external_id;
        }
    }
    return {};
}

/// The unescaped cell value for one selected field.
std::string cell(const Film& f, io::Field field) {
    using io::Field;
    switch (field) {
        case Field::Title:         return f.title;
        case Field::OriginalTitle: return f.original_title;
        case Field::Year:          return opt_int(f.year);
        case Field::Runtime:       return opt_int(f.runtime_minutes);
        case Field::Synopsis:      return f.synopsis;
        case Field::Genres:        return join_list(f.genres);
        case Field::Cast:          return join_names(f, CreditRole::Actor);
        case Field::Directors:     return join_names(f, CreditRole::Director);
        case Field::Writers:       return join_names(f, CreditRole::Writer);
        case Field::Composers:     return join_names(f, CreditRole::Composer);
        case Field::Topics:        return join_list(f.topics);
        case Field::Groups:        return join_list(f.groups);
        case Field::ImdbId:        return imdb_ext_id(f);
        case Field::ImdbRating:    return opt_double(query::source_rating(f, "imdb"));
        case Field::UserRating:    return opt_double(f.user.personal_rating);
        case Field::WatchDate:     return f.user.date_watched.value_or("");
        case Field::WatchCount:    return std::to_string(f.user.watch_count);
        case Field::Owned:         return f.user.owned ? "true" : "false";
        case Field::Wishlist:      return f.user.wishlist ? "true" : "false";
        case Field::Favorite:      return f.user.favorite ? "true" : "false";
        case Field::Comments:      return f.user.notes;
        case Field::VideoFile:     return f.video ? f.video->path : std::string();
    }
    return {};
}

}  // namespace

void write_csv(std::ostream& os, const std::vector<const Film*>& films,
               const io::FieldSet& selection) {
    const std::vector<io::Field> cols = selection.fields();
    os << "id";
    for (io::Field c : cols) {
        os << ',' << io::token_of(c);
    }
    os << "\r\n";
    for (const Film* f : films) {
        os << f->id;
        for (io::Field c : cols) {
            os << ',' << escape(cell(*f, c));
        }
        os << "\r\n";
    }
}

}  // namespace pfdb
