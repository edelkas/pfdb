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

}  // namespace pfdb
