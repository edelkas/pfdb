#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "pfdb/types.hpp"

namespace pfdb::gui {

/// A decoded cover image uploaded to an OpenGL texture.
struct Texture {
    unsigned int id = 0;  // GLuint (0 = none)
    int width = 0;
    int height = 0;
};

/// Decodes cover-art blobs (JPEG/PNG via stb_image) into GL textures, cached by
/// film id so a cover is uploaded once. `get` returns the cached texture,
/// decoding `bytes` on first use; `invalidate` drops one film's texture (e.g.
/// after its cover changes).
class TextureCache {
public:
    ~TextureCache();

    /// Cached texture for `film_id`, decoding `bytes` if not yet cached. Returns
    /// a texture with id 0 if `bytes` is empty or fails to decode.
    const Texture& get(Id film_id, const std::string& bytes);

    bool has(Id film_id) const { return cache_.count(film_id) != 0; }
    void invalidate(Id film_id);
    void clear();

private:
    std::unordered_map<Id, Texture> cache_;
};

}  // namespace pfdb::gui
