#include "app/cover.hpp"

#include <algorithm>
#include <cctype>

#include "db/repository.hpp"
#include "net/http_client.hpp"

namespace pfdb::app {

std::string mime_from_url(std::string_view url) {
    // Look only at the path's extension, ignoring any query string.
    std::string_view path = url.substr(0, url.find('?'));
    const auto dot = path.rfind('.');
    if (dot != std::string_view::npos) {
        std::string ext(path.substr(dot + 1));
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == "png") return "image/png";
        if (ext == "webp") return "image/webp";
        if (ext == "gif") return "image/gif";
    }
    return "image/jpeg";
}

bool fetch_and_store_cover(db::Repository& repo, net::IHttpClient& http, Id film_id,
                           const std::string& cover_url) {
    if (cover_url.empty()) {
        return false;
    }
    net::HttpResponse resp = http.get(cover_url);
    if (!resp.ok() || resp.body.empty()) {
        return false;
    }
    repo.set_cover(film_id, mime_from_url(cover_url), resp.body);
    return true;
}

}  // namespace pfdb::app
