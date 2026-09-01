#pragma once

#include <string>
#include <utility>
#include <vector>

namespace pfdb::net {

/// One HTTP request/response header.
using Header = std::pair<std::string, std::string>;
using Headers = std::vector<Header>;

struct HttpResponse {
    long status = 0;
    std::string body;

    bool ok() const noexcept { return status >= 200 && status < 300; }
};

/// Abstraction over an HTTP client so that sources can be unit-tested with a
/// mock instead of hitting the network. Fetchers depend on this interface; the
/// real cpr-backed implementation lives in the .cpp so cpr stays an
/// implementation detail of pfdb_core.
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    virtual HttpResponse get(const std::string& url, const Headers& headers = {}) = 0;
    virtual HttpResponse post(const std::string& url, const std::string& body,
                              const Headers& headers = {}) = 0;
};

/// IHttpClient backed by cpr/libcurl. Adds a default User-Agent and a request
/// timeout; callers supply any request-specific headers.
class CprHttpClient : public IHttpClient {
public:
    explicit CprHttpClient(long timeout_ms = 15000);

    HttpResponse get(const std::string& url, const Headers& headers = {}) override;
    HttpResponse post(const std::string& url, const std::string& body,
                      const Headers& headers = {}) override;

private:
    long timeout_ms_;
};

}  // namespace pfdb::net
