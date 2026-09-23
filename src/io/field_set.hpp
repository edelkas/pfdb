#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pfdb::config {
class Config;
}

namespace pfdb::io {

/// A PFDB-compatible field that can be selected for import or export. This is a
/// curated, source-agnostic set: not every EMDB field maps here, and fields with
/// no importer today (e.g. from other sources) can still be exported.
enum class Field {
    Title,
    OriginalTitle,
    Year,
    Runtime,
    Synopsis,
    Genres,
    Cast,
    Directors,
    Writers,
    Composers,
    Topics,
    Groups,
    ImdbId,
    ImdbRating,
    UserRating,
    WatchDate,
    WatchCount,
    Owned,
    Wishlist,
    Favorite,
    Comments,
    VideoFile,
};

/// Thrown for an unknown field token or preset name.
class FieldError : public std::runtime_error {
public:
    explicit FieldError(const std::string& message) : std::runtime_error(message) {}
};

/// A set of selected fields. Iteration and serialization are in canonical
/// (declaration) order, independent of insertion order.
class FieldSet {
public:
    FieldSet() = default;

    void add(Field f);
    bool contains(Field f) const;
    bool empty() const;

    /// Selected fields, in canonical order.
    std::vector<Field> fields() const;
    /// Selected fields' tokens, in canonical order.
    std::vector<std::string> tokens() const;

    // Built-in presets.
    static FieldSet all();
    static FieldSet userdata();  ///< Identity anchor + user-specific fields.
    static FieldSet metadata();  ///< Everything redownloadable (all minus user).

private:
    std::vector<bool> bits_ = std::vector<bool>(kCount(), false);
    static std::size_t kCount();
};

/// The canonical, ordered list of every field.
const std::vector<Field>& all_fields();

/// Field <-> kebab-case token ("original-title", "watch-count", ...).
std::string_view token_of(Field f);
std::optional<Field> field_from_token(std::string_view token);

/// Parse a comma-separated token list into a FieldSet. Throws FieldError.
FieldSet parse_field_list(std::string_view csv);

/// The three built-in preset names, or nullopt if `name` is not built-in.
std::optional<FieldSet> builtin_preset(std::string_view name);
bool is_builtin_preset(std::string_view name);

/// Resolve a selection: `--fields` wins if set, else `--preset` (built-in or a
/// user preset from `config`), else `fallback`. Throws FieldError on a bad token
/// or unknown preset. `fields_csv`/`preset` empty means "not given".
FieldSet resolve_selection(std::string_view preset, std::string_view fields_csv,
                           const config::Config& config, const FieldSet& fallback);

}  // namespace pfdb::io
