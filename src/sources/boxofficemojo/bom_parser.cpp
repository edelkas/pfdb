#include "sources/boxofficemojo/bom_parser.hpp"

#include <cctype>
#include <string>

#include "parse/html.hpp"

namespace pfdb::sources::boxofficemojo {
namespace {

using parse::HtmlDocument;
using parse::Node;

std::string trim(std::string_view s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && (std::isspace(static_cast<unsigned char>(s[b])) != 0)) {
        ++b;
    }
    while (e > b && (std::isspace(static_cast<unsigned char>(s[e - 1])) != 0)) {
        --e;
    }
    return std::string(s.substr(b, e - b));
}

/// Parse a money string like "$150,000,000" into an integer number of dollars.
std::optional<std::int64_t> parse_money(std::string_view s) {
    std::string digits;
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
            digits.push_back(c);
        }
    }
    if (digits.empty()) {
        return std::nullopt;
    }
    try {
        return static_cast<std::int64_t>(std::stoll(digits));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace

Financials parse_financials(std::string_view html) {
    HtmlDocument doc(html);
    Financials fin;

    // Budget: the labelled summary row `<span>Budget</span> ... <span class="money">`.
    for (const auto& row : doc.select("div.a-section.a-spacing-none")) {
        const auto label = row.select_first("span");
        if (!label || trim(label->text()) != "Budget") {
            continue;
        }
        if (const auto money = row.select_first(".money")) {
            fin.budget = parse_money(money->text());
        }
        break;
    }

    // Grosses: the performance-summary table lists Domestic, International, then
    // Worldwide money in that order; the worldwide total is the last of them.
    if (const auto summary = doc.select_first(".mojo-performance-summary-table")) {
        const auto monies = summary->select(".money");
        if (!monies.empty()) {
            fin.gross = parse_money(monies.back().text());
        }
    }

    return fin;
}

}  // namespace pfdb::sources::boxofficemojo
