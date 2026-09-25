#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "pfdb/credit.hpp"
#include "pfdb/rating.hpp"
#include "pfdb/source_ref.hpp"
#include "pfdb/types.hpp"
#include "pfdb/user_data.hpp"

namespace pfdb {

/// The central aggregate of PFDB: one film in the user's collection, holding
/// both externally sourced metadata and the user's own data.
///
/// A Film is a plain value: it can be constructed, copied, compared, and
/// serialized freely. Persistence (src/db) and the in-memory index
/// (src/model) operate on Films; they do not live inside it.
///
/// Note on provenance: M1 tracks provenance at the film level via
/// `source_refs`. Per-field provenance (which source supplied the title vs.
/// the runtime) is introduced with multi-source merge in a later milestone.
struct Film {
    Id id = kInvalidId;

    // --- Core metadata ---
    std::string title;
    /// Original-language title, when it differs from `title`.
    std::string original_title;
    std::optional<int> year;
    std::optional<int> runtime_minutes;
    std::string synopsis;
    std::vector<std::string> genres;

    // --- Financials (USD; scraped from BoxOfficeMojo) ---
    std::optional<std::int64_t> budget;
    std::optional<std::int64_t> gross;  ///< Worldwide total gross.

    // --- Localized (FilmAffinity) metadata ---
    /// Spanish title, when a source (FilmAffinity) provides one.
    std::string spanish_title;
    /// Spanish synopsis.
    std::string spanish_synopsis;
    /// Number of user reviews behind the score (FilmAffinity "críticas" count).
    std::optional<int> review_count;
    /// FilmAffinity "temas": finer-grained tags than genres (e.g. "Neo-noir").
    std::vector<std::string> topics;
    /// FilmAffinity groups/sagas (e.g. "Adaptaciones de Philip K. Dick").
    std::vector<std::string> groups;

    // --- People ---
    std::vector<Credit> credits;

    // --- Scores from sources (and optionally the user) ---
    std::vector<Rating> ratings;

    // --- External identities this film was enriched from ---
    std::vector<SourceRef> source_refs;

    // --- User-specific data ---
    UserData user;
    std::optional<VideoFileInfo> video;

    // --- Bookkeeping (UTC Unix seconds) ---
    std::optional<UnixSeconds> created_at;
    std::optional<UnixSeconds> updated_at;

    friend bool operator==(const Film&, const Film&) = default;
};

}  // namespace pfdb
