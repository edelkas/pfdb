#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "pfdb/film.hpp"
#include "pfdb/types.hpp"
#include "query/eval_context.hpp"

namespace pfdb::query {

/// The value shape of a queryable film field. The field's kind decides which
/// filter types and operators are valid for it, and how sorting compares it.
enum class FieldKind {
    Text,        ///< A single string (title, synopsis, ...). Text filters + sort.
    Number,      ///< A single optional number (year, runtime, ...). Range filters + sort.
    Date,        ///< A single optional ISO date (date_watched). Range filters + sort.
    StringList,  ///< A list of strings matched exactly (genre, topic, group). Inclusion.
    NameList,    ///< A list of person names (cast, director, writer). Name inclusion.
    IdList,      ///< A list of ids (cast_id, related_to, ...). Exact-id inclusion.
};

/// A resolved field: its canonical name plus kind.
struct FieldInfo {
    std::string name;
    FieldKind kind;
};

/// Resolve a user-supplied field token (case-insensitive; hyphens and a
/// trailing plural 's' are tolerated, e.g. "Original-Title", "genres") to its
/// canonical field, or nullopt if unknown.
std::optional<FieldInfo> find_field(std::string_view token);

/// Every known field, for help text and error messages.
const std::vector<FieldInfo>& all_fields();

/// For a NameList field ("cast"), the matching IdList field ("cast_id").
/// Returns empty for non-name fields.
std::string id_field_for(std::string_view name_field);

/// The credit role a NameList field draws from ("cast" -> Actor).
CreditRole role_for_name_field(std::string_view name_field);

// --- Extraction (field name is assumed canonical, from find_field) ---

/// The score a given source reports for `film`, if present (e.g. "imdb").
std::optional<double> source_rating(const Film& film, std::string_view source);

std::optional<std::string> extract_text(std::string_view field, const Film& film);
std::optional<double> extract_number(std::string_view field, const Film& film);
std::optional<std::string> extract_date(std::string_view field, const Film& film);
std::vector<std::string> extract_strings(std::string_view field, const Film& film);
std::vector<Id> extract_ids(std::string_view field, const Film& film,
                            const EvalContext& ctx);

}  // namespace pfdb::query
