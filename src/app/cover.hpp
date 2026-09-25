#pragma once

#include <string>
#include <string_view>

#include "pfdb/types.hpp"

namespace pfdb::db {
class Repository;
}
namespace pfdb::net {
class IHttpClient;
}

namespace pfdb::app {

/// Guess an image media type ("image/jpeg", ...) from a URL or filename's
/// extension, defaulting to "image/jpeg".
std::string mime_from_url(std::string_view url);

/// Download the image at `cover_url` and store it as `film_id`'s cover. Returns
/// false (storing nothing) if the URL is empty or the download fails/empties.
bool fetch_and_store_cover(db::Repository& repo, net::IHttpClient& http, Id film_id,
                           const std::string& cover_url);

}  // namespace pfdb::app
