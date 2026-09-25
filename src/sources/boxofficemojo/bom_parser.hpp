#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace pfdb::sources::boxofficemojo {

/// Financial figures scraped from a BoxOfficeMojo title page (USD).
struct Financials {
    std::optional<std::int64_t> budget;
    std::optional<std::int64_t> gross;  ///< Worldwide total gross.
};

/// Parse a BoxOfficeMojo title page (boxofficemojo.com/title/<ttid>/). Budget
/// comes from its labelled row; the worldwide gross is the last of the
/// Domestic/International/Worldwide figures in the performance-summary table.
/// Missing figures are left unset (this is scraping — kept fixture-tested).
Financials parse_financials(std::string_view html);

}  // namespace pfdb::sources::boxofficemojo
