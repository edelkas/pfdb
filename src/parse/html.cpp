#include "parse/html.hpp"

#include <stdexcept>

#include <lexbor/css/css.h>
#include <lexbor/css/selectors/selectors.h>
#include <lexbor/dom/dom.h>
#include <lexbor/html/html.h>
#include <lexbor/selectors/selectors.h>

namespace pfdb::parse {
namespace {

const lxb_char_t* as_lxb(std::string_view s) {
    return reinterpret_cast<const lxb_char_t*>(s.data());
}

// Callback for lxb_selectors_find: append each matched node to a vector.
lxb_status_t collect_cb(lxb_dom_node_t* node, lxb_css_selector_specificity_t /*spec*/,
                        void* ctx) {
    auto* out = static_cast<std::vector<void*>*>(ctx);
    out->push_back(node);
    return LXB_STATUS_OK;
}

}  // namespace

struct HtmlDocument::Impl {
    lxb_html_document_t* document = nullptr;

    ~Impl() {
        if (document != nullptr) {
            lxb_html_document_destroy(document);
        }
    }
};

HtmlDocument::HtmlDocument(std::string_view html) : impl_(std::make_unique<Impl>()) {
    impl_->document = lxb_html_document_create();
    if (impl_->document == nullptr) {
        throw std::runtime_error("html: failed to create document");
    }
    if (lxb_html_document_parse(impl_->document, as_lxb(html), html.size()) !=
        LXB_STATUS_OK) {
        throw std::runtime_error("html: failed to parse document");
    }
}

HtmlDocument::~HtmlDocument() = default;
HtmlDocument::HtmlDocument(HtmlDocument&&) noexcept = default;
HtmlDocument& HtmlDocument::operator=(HtmlDocument&&) noexcept = default;

std::vector<Node> HtmlDocument::query(void* root_node, std::string_view css,
                                      bool first_only) const {
    std::vector<Node> result;
    if (root_node == nullptr) {
        return result;
    }

    // Per-query parser + selectors, matching lexbor's documented lifecycle.
    lxb_css_parser_t* parser = lxb_css_parser_create();
    if (parser == nullptr || lxb_css_parser_init(parser, nullptr) != LXB_STATUS_OK) {
        if (parser != nullptr) {
            lxb_css_parser_destroy(parser, true);
        }
        return result;
    }
    lxb_selectors_t* selectors = lxb_selectors_create();
    if (selectors == nullptr || lxb_selectors_init(selectors) != LXB_STATUS_OK) {
        lxb_selectors_destroy(selectors, true);
        lxb_css_parser_destroy(parser, true);
        return result;
    }
    lxb_selectors_opt_set(selectors, first_only ? LXB_SELECTORS_OPT_MATCH_FIRST
                                                 : LXB_SELECTORS_OPT_DEFAULT);

    lxb_css_selector_list_t* list =
        lxb_css_selectors_parse(parser, as_lxb(css), css.size());
    if (list != nullptr) {
        std::vector<void*> nodes;
        lxb_selectors_find(selectors, static_cast<lxb_dom_node_t*>(root_node), list,
                           collect_cb, &nodes);
        lxb_css_selector_list_destroy_memory(list);

        result.reserve(nodes.size());
        for (void* n : nodes) {
            result.push_back(Node(n, this));
        }
    }

    lxb_selectors_destroy(selectors, true);
    lxb_css_parser_destroy(parser, true);
    return result;
}

std::vector<Node> HtmlDocument::select(std::string_view css) const {
    return query(lxb_dom_interface_node(impl_->document), css, /*first_only=*/false);
}

std::optional<Node> HtmlDocument::select_first(std::string_view css) const {
    auto nodes = query(lxb_dom_interface_node(impl_->document), css, /*first_only=*/true);
    if (nodes.empty()) {
        return std::nullopt;
    }
    return nodes.front();
}

std::vector<Node> Node::select(std::string_view css) const {
    return doc_->query(node_, css, /*first_only=*/false);
}

std::optional<Node> Node::select_first(std::string_view css) const {
    auto nodes = doc_->query(node_, css, /*first_only=*/true);
    if (nodes.empty()) {
        return std::nullopt;
    }
    return nodes.front();
}

std::optional<std::string> Node::attr(std::string_view name) const {
    auto* element = lxb_dom_interface_element(static_cast<lxb_dom_node_t*>(node_));
    size_t len = 0;
    const lxb_char_t* value =
        lxb_dom_element_get_attribute(element, as_lxb(name), name.size(), &len);
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char*>(value), len);
}

std::string Node::text() const {
    auto* node = static_cast<lxb_dom_node_t*>(node_);
    size_t len = 0;
    lxb_char_t* content = lxb_dom_node_text_content(node, &len);
    if (content == nullptr) {
        return {};
    }
    std::string out(reinterpret_cast<const char*>(content), len);
    lxb_dom_document_destroy_text(node->owner_document, content);
    return out;
}

}  // namespace pfdb::parse
