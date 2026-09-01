#pragma once

#include <string>
#include <utility>
#include <vector>

#include "net/http_client.hpp"

namespace pfdb::test {

/// An IHttpClient that returns canned responses matched by URL substring and
/// records every request, so source classes can be driven entirely offline.
class MockHttpClient : public net::IHttpClient {
public:
    struct Call {
        std::string method;
        std::string url;
        std::string body;
    };

    std::vector<Call> calls;

    /// Register a canned response for any URL containing `url_contains`.
    void on(std::string url_contains, long status, std::string body) {
        rules_.push_back({std::move(url_contains), net::HttpResponse{status, std::move(body)}});
    }

    net::HttpResponse get(const std::string& url, const net::Headers& = {}) override {
        calls.push_back({"GET", url, ""});
        return match(url);
    }

    net::HttpResponse post(const std::string& url, const std::string& body,
                           const net::Headers& = {}) override {
        calls.push_back({"POST", url, body});
        return match(url);
    }

private:
    struct Rule {
        std::string url_contains;
        net::HttpResponse response;
    };
    std::vector<Rule> rules_;

    net::HttpResponse match(const std::string& url) const {
        for (const auto& rule : rules_) {
            if (url.find(rule.url_contains) != std::string::npos) {
                return rule.response;
            }
        }
        return net::HttpResponse{404, ""};  // no rule matched
    }
};

}  // namespace pfdb::test
