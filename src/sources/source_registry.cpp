#include "sources/source_registry.hpp"

#include "sources/imdb/imdb_source.hpp"

namespace pfdb::sources {

std::unique_ptr<ISource> make_source(std::string_view id, net::IHttpClient& http) {
    if (id == "imdb") {
        return std::make_unique<imdb::ImdbSource>(http);
    }
    return nullptr;
}

std::vector<std::string> available_sources() {
    return {"imdb"};
}

}  // namespace pfdb::sources
