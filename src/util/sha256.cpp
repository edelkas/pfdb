#include "util/sha256.hpp"

#include <array>
#include <cstdint>

namespace pfdb::util {
namespace {

constexpr std::array<std::uint32_t, 64> kK = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

constexpr std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

class Sha256 {
public:
    void update(const unsigned char* data, std::size_t len) {
        total_ += len;
        for (std::size_t i = 0; i < len; ++i) {
            buf_[buf_len_++] = data[i];
            if (buf_len_ == 64) {
                process(buf_.data());
                buf_len_ = 0;
            }
        }
    }

    std::array<unsigned char, 32> finish() {
        const std::uint64_t bit_len = total_ * 8;
        unsigned char pad = 0x80;
        update(&pad, 1);
        pad = 0x00;
        while (buf_len_ != 56) {
            update(&pad, 1);
        }
        std::array<unsigned char, 8> len_be{};
        for (int i = 0; i < 8; ++i) {
            len_be[static_cast<std::size_t>(i)] =
                static_cast<unsigned char>((bit_len >> (56 - 8 * i)) & 0xff);
        }
        update(len_be.data(), len_be.size());

        std::array<unsigned char, 32> out{};
        for (std::size_t i = 0; i < 8; ++i) {
            out[i * 4 + 0] = static_cast<unsigned char>((h_[i] >> 24) & 0xff);
            out[i * 4 + 1] = static_cast<unsigned char>((h_[i] >> 16) & 0xff);
            out[i * 4 + 2] = static_cast<unsigned char>((h_[i] >> 8) & 0xff);
            out[i * 4 + 3] = static_cast<unsigned char>(h_[i] & 0xff);
        }
        return out;
    }

private:
    void process(const unsigned char* block) {
        std::array<std::uint32_t, 64> w{};
        for (std::size_t i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                   (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                   (static_cast<std::uint32_t>(block[i * 4 + 3]));
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const std::uint32_t s0 =
                rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 =
                rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h_[0];
        std::uint32_t b = h_[1];
        std::uint32_t c = h_[2];
        std::uint32_t d = h_[3];
        std::uint32_t e = h_[4];
        std::uint32_t f = h_[5];
        std::uint32_t g = h_[6];
        std::uint32_t h = h_[7];

        for (std::size_t i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = h + s1 + ch + kK[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        h_[0] += a;
        h_[1] += b;
        h_[2] += c;
        h_[3] += d;
        h_[4] += e;
        h_[5] += f;
        h_[6] += g;
        h_[7] += h;
    }

    std::array<std::uint32_t, 8> h_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                       0xa54ff53a, 0x510e527f, 0x9b05688c,
                                       0x1f83d9ab, 0x5be0cd19};
    std::array<unsigned char, 64> buf_{};
    std::size_t buf_len_ = 0;
    std::uint64_t total_ = 0;
};

}  // namespace

std::string sha256_hex(std::string_view data) {
    Sha256 ctx;
    ctx.update(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    const std::array<unsigned char, 32> digest = ctx.finish();

    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (unsigned char byte : digest) {
        out.push_back(kHex[byte >> 4]);
        out.push_back(kHex[byte & 0x0f]);
    }
    return out;
}

}  // namespace pfdb::util
