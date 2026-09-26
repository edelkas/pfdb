// stb_image provides the JPEG/PNG decoder; define its implementation here only.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"

#include "gui/texture.hpp"

#include <GLFW/glfw3.h>  // drags in the system OpenGL headers

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace pfdb::gui {
namespace {

Texture upload(const std::string& bytes) {
    Texture tex;
    if (bytes.empty()) {
        return tex;
    }
    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()),
        &w, &h, &channels, 4);
    if (pixels == nullptr) {
        return tex;
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);

    tex.id = id;
    tex.width = w;
    tex.height = h;
    return tex;
}

}  // namespace

TextureCache::~TextureCache() { clear(); }

const Texture& TextureCache::get(Id film_id, const std::string& bytes) {
    auto it = cache_.find(film_id);
    if (it == cache_.end()) {
        it = cache_.emplace(film_id, upload(bytes)).first;
    }
    return it->second;
}

void TextureCache::invalidate(Id film_id) {
    auto it = cache_.find(film_id);
    if (it != cache_.end()) {
        if (it->second.id != 0) {
            GLuint id = it->second.id;
            glDeleteTextures(1, &id);
        }
        cache_.erase(it);
    }
}

void TextureCache::clear() {
    for (auto& [id, tex] : cache_) {
        if (tex.id != 0) {
            GLuint gl = tex.id;
            glDeleteTextures(1, &gl);
        }
    }
    cache_.clear();
}

}  // namespace pfdb::gui
