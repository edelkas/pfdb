#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// A small C++ wrapper over lexbor for the bits PFDB needs: parse an HTML
// document and query it with CSS selectors. Used by HTML-scraping sources
// (FilmAffinity). lexbor stays entirely inside html.cpp.
namespace pfdb::parse {

class HtmlDocument;

/// A non-owning handle to an element within a HtmlDocument. Valid only while the
/// owning document is alive.
class Node {
public:
    /// All descendants matching the CSS selector, in document order.
    std::vector<Node> select(std::string_view css) const;
    /// The first descendant matching the selector, if any.
    std::optional<Node> select_first(std::string_view css) const;

    /// Value of attribute `name`, or nullopt if the attribute is absent.
    std::optional<std::string> attr(std::string_view name) const;

    /// Concatenated text content of this node and its descendants.
    std::string text() const;

    bool valid() const noexcept { return node_ != nullptr; }

private:
    Node(void* node, const HtmlDocument* doc) : node_(node), doc_(doc) {}

    void* node_ = nullptr;         // lxb_dom_node_t*
    const HtmlDocument* doc_ = nullptr;
    friend class HtmlDocument;
};

/// Owns a parsed HTML document.
class HtmlDocument {
public:
    /// Parse `html`. Throws std::runtime_error if parsing fails.
    explicit HtmlDocument(std::string_view html);
    ~HtmlDocument();

    HtmlDocument(HtmlDocument&&) noexcept;
    HtmlDocument& operator=(HtmlDocument&&) noexcept;
    HtmlDocument(const HtmlDocument&) = delete;
    HtmlDocument& operator=(const HtmlDocument&) = delete;

    /// Query from the document body.
    std::vector<Node> select(std::string_view css) const;
    std::optional<Node> select_first(std::string_view css) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    friend class Node;

    // Run a selector query rooted at `root_node` (lxb_dom_node_t*).
    std::vector<Node> query(void* root_node, std::string_view css, bool first_only) const;
};

}  // namespace pfdb::parse
