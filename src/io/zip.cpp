#include "io/zip.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <miniz.h>

namespace pfdb::io {
namespace fs = std::filesystem;

void extract_zip(const std::string& zip_path, const std::string& dest_dir) {
    mz_zip_archive zip{};
    if (mz_zip_reader_init_file(&zip, zip_path.c_str(), 0) == MZ_FALSE) {
        throw std::runtime_error("could not open zip archive '" + zip_path + "'");
    }

    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    try {
        for (mz_uint i = 0; i < count; ++i) {
            mz_zip_archive_file_stat st{};
            if (mz_zip_reader_file_stat(&zip, i, &st) == MZ_FALSE) {
                throw std::runtime_error("corrupt entry in zip '" + zip_path + "'");
            }

            const fs::path out = fs::path(dest_dir) / fs::path(st.m_filename);
            if (mz_zip_reader_is_file_a_directory(&zip, i) != MZ_FALSE) {
                fs::create_directories(out);
                continue;
            }

            if (out.has_parent_path()) {
                fs::create_directories(out.parent_path());
            }

            // Extract into memory, then write the file, so we control encoding
            // and paths rather than relying on miniz's own path handling.
            std::size_t size = 0;
            void* data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
            if (data == nullptr) {
                throw std::runtime_error("failed to extract '" +
                                         std::string(st.m_filename) + "' from '" +
                                         zip_path + "'");
            }
            std::ofstream of(out, std::ios::binary | std::ios::trunc);
            if (of) {
                of.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
            }
            mz_free(data);
            if (!of) {
                throw std::runtime_error("could not write '" + out.string() + "'");
            }
        }
    } catch (...) {
        mz_zip_reader_end(&zip);
        throw;
    }
    mz_zip_reader_end(&zip);
}

}  // namespace pfdb::io
