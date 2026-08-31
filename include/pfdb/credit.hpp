#pragma once

#include <string>
#include <string_view>

#include "pfdb/types.hpp"

namespace pfdb {

/// A film-industry person (actor, director, writer, ...).
/// In M1 this is a lightweight value; richer person metadata (birth date,
/// external source ids) arrives with the source/enrichment milestones.
struct Person {
    Id id = kInvalidId;
    std::string name;

    friend bool operator==(const Person&, const Person&) = default;
};

/// The kind of contribution a person made to a film.
enum class CreditRole {
    Director,
    Writer,
    Actor,
    Producer,
    Composer,
    Cinematographer,
    Editor,
    Other,
};

/// A single person's involvement in a film.
struct Credit {
    Person person;
    CreditRole role = CreditRole::Other;
    /// Character played, for acting credits. Empty otherwise.
    std::string character;
    /// Billing/display order within the film (0-based). Lower shows first.
    int order = 0;

    friend bool operator==(const Credit&, const Credit&) = default;
};

/// Stable lowercase token for a role, used for persistence and JSON.
std::string_view to_string(CreditRole role) noexcept;

/// Parse a role token produced by to_string(). Unknown tokens map to Other.
CreditRole credit_role_from_string(std::string_view token) noexcept;

}  // namespace pfdb
