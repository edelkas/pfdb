#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "net/http_client.hpp"
#include "sources/source.hpp"

namespace pfdb::sources {

/// Construct the source identified by `id` (e.g. "imdb"), wired to `http`.
/// Returns nullptr for an unknown id. This is the single place new sources are
/// registered; the rest of the app only knows the ISource interface.
///
/// The returned source holds a reference to `http`, which must outlive it.
std::unique_ptr<ISource> make_source(std::string_view id, net::IHttpClient& http);

/// Ids of all sources this build supports, for help text and validation.
std::vector<std::string> available_sources();

}  // namespace pfdb::sources
