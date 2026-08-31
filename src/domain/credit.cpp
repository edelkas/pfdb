#include "pfdb/credit.hpp"

#include <array>
#include <utility>

namespace pfdb {
namespace {

// Single source of truth mapping each role to its stable token. Keep the
// order aligned with the CreditRole enum so lookups by role are O(1).
constexpr std::array<std::pair<CreditRole, std::string_view>, 8> kRoleTokens{{
    {CreditRole::Director, "director"},
    {CreditRole::Writer, "writer"},
    {CreditRole::Actor, "actor"},
    {CreditRole::Producer, "producer"},
    {CreditRole::Composer, "composer"},
    {CreditRole::Cinematographer, "cinematographer"},
    {CreditRole::Editor, "editor"},
    {CreditRole::Other, "other"},
}};

}  // namespace

std::string_view to_string(CreditRole role) noexcept {
    for (const auto& [value, token] : kRoleTokens) {
        if (value == role) {
            return token;
        }
    }
    return "other";
}

CreditRole credit_role_from_string(std::string_view token) noexcept {
    for (const auto& [value, tok] : kRoleTokens) {
        if (tok == token) {
            return value;
        }
    }
    return CreditRole::Other;
}

}  // namespace pfdb
