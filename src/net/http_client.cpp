#include "net/http_client.hpp"

#include <chrono>

#include <cpr/cpr.h>

namespace pfdb::net {
namespace {

// A realistic desktop-Chrome User-Agent. Some sources (IMDb) reject requests
// that don't look like a browser.
constexpr const char* kDefaultUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";

cpr::Header to_cpr_header(const Headers& headers) {
    cpr::Header out;
    out["User-Agent"] = kDefaultUserAgent;  // caller may override below
    for (const auto& [key, value] : headers) {
        out[key] = value;
    }
    return out;
}

}  // namespace

CprHttpClient::CprHttpClient(long timeout_ms) : timeout_ms_(timeout_ms) {}

HttpResponse CprHttpClient::get(const std::string& url, const Headers& headers) {
    cpr::Response r = cpr::Get(cpr::Url{url}, to_cpr_header(headers),
                               cpr::Timeout{std::chrono::milliseconds{timeout_ms_}});
    return HttpResponse{static_cast<long>(r.status_code), r.text};
}

HttpResponse CprHttpClient::post(const std::string& url, const std::string& body,
                                 const Headers& headers) {
    cpr::Response r = cpr::Post(cpr::Url{url}, cpr::Body{body}, to_cpr_header(headers),
                                cpr::Timeout{std::chrono::milliseconds{timeout_ms_}});
    return HttpResponse{static_cast<long>(r.status_code), r.text};
}

}  // namespace pfdb::net
