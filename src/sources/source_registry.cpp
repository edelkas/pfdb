#include "sources/source_registry.hpp"

#include "sources/boxofficemojo/bom_source.hpp"
#include "sources/filmaffinity/fa_source.hpp"
#include "sources/imdb/imdb_source.hpp"

namespace pfdb::sources {

std::unique_ptr<ISource> make_source(std::string_view id, net::IHttpClient& http) {
    if (id == "imdb") {
        return std::make_unique<imdb::ImdbSource>(http);
    }
    if (id == "filmaffinity") {
        return std::make_unique<filmaffinity::FaSource>(http);
    }
    if (id == "boxofficemojo") {
        return std::make_unique<boxofficemojo::BomSource>(http);
    }
    return nullptr;
}

std::vector<std::string> available_sources() {
    return {"imdb", "filmaffinity", "boxofficemojo"};
}

}  // namespace pfdb::sources
