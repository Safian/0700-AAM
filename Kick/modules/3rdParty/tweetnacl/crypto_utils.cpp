// SPDX-License-Identifier: AGPL-3.0-or-later
#include "crypto_utils.h"
#include "tweetnacl.h"
#include "sha256.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>

extern "C" void randombytes(unsigned char *ptr, unsigned long long length) {
    if (!ptr || length == 0) return;
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (urandom.is_open()) {
        urandom.read(reinterpret_cast<char*>(ptr), static_cast<std::streamsize>(length));
        if (urandom.gcount() == static_cast<std::streamsize>(length)) {
            return;
        }
    }
    std::random_device rd;
    for (unsigned long long i = 0; i < length; ++i) {
        ptr[i] = static_cast<unsigned char>(rd() & 0xFF);
    }
}

namespace kick::crypto {

std::string to_hex(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    return oss.str();
}

std::string to_hex(const std::vector<uint8_t>& data) {
    return to_hex(data.data(), data.size());
}

static int hex_char_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool from_hex(const std::string& hex, std::vector<uint8_t>& out) {
    if (hex.size() % 2 != 0) return false;
    out.clear();
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = hex_char_to_val(hex[i]);
        int lo = hex_char_to_val(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            out.clear();
            return false;
        }
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

std::string sha256_buffer(const void* data, size_t len) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, static_cast<const uint8_t*>(data), len);
    uint8_t hash[32];
    sha256_final(&ctx, hash);
    return to_hex(hash, 32);
}

std::string sha256_string(const std::string& str) {
    return sha256_buffer(str.data(), str.size());
}

std::string sha256_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";

    SHA256_CTX ctx;
    sha256_init(&ctx);

    constexpr size_t buffer_size = 65536;
    std::vector<uint8_t> buffer(buffer_size);

    while (file.good()) {
        file.read(reinterpret_cast<char*>(buffer.data()), buffer_size);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            sha256_update(&ctx, buffer.data(), static_cast<size_t>(bytesRead));
        }
    }

    uint8_t hash[32];
    sha256_final(&ctx, hash);
    return to_hex(hash, 32);
}

bool ed25519_create_keypair_from_seed(const uint8_t* seed32, uint8_t* outPublicKey32, uint8_t* outSecretKey64) {
    if (!seed32 || !outPublicKey32 || !outSecretKey64) return false;
    std::memcpy(outSecretKey64, seed32, 32);
    return crypto_sign_keypair(outPublicKey32, outSecretKey64) == 0;
}

bool ed25519_create_keypair(uint8_t* outPublicKey32, uint8_t* outSecretKey64) {
    if (!outPublicKey32 || !outSecretKey64) return false;
    return crypto_sign_keypair(outPublicKey32, outSecretKey64) == 0;
}

bool ed25519_sign_detached(const uint8_t* message, size_t messageLen,
                           const uint8_t* secretKey64, uint8_t* outSignature64) {
    if (!secretKey64 || !outSignature64) return false;
    std::vector<uint8_t> sm(messageLen + 64);
    unsigned long long smlen = 0;

    int ret = crypto_sign(sm.data(), &smlen, message, messageLen, secretKey64);
    if (ret != 0 || smlen != messageLen + 64) return false;

    std::memcpy(outSignature64, sm.data(), 64);
    return true;
}

bool ed25519_verify_detached(const uint8_t* message, size_t messageLen,
                             const uint8_t* publicKey32, const uint8_t* signature64) {
    if (!publicKey32 || !signature64) return false;

    std::vector<uint8_t> sm(messageLen + 64);
    std::memcpy(sm.data(), signature64, 64);
    if (messageLen > 0 && message) {
        std::memcpy(sm.data() + 64, message, messageLen);
    }

    std::vector<uint8_t> m(messageLen + 64);
    unsigned long long mlen = 0;

    int ret = crypto_sign_open(m.data(), &mlen, sm.data(), messageLen + 64, publicKey32);
    return (ret == 0 && mlen == messageLen);
}

bool ed25519_sign_string(const std::string& message, const std::string& secretKeyHex, std::string& outSignatureHex) {
    std::vector<uint8_t> sk;
    if (!from_hex(secretKeyHex, sk) || sk.size() != ED25519_SECRET_KEY_SIZE) return false;

    uint8_t sig[ED25519_SIGNATURE_SIZE];
    if (!ed25519_sign_detached(reinterpret_cast<const uint8_t*>(message.data()), message.size(), sk.data(), sig)) {
        return false;
    }

    outSignatureHex = to_hex(sig, ED25519_SIGNATURE_SIZE);
    return true;
}

bool ed25519_verify_string(const std::string& message, const std::string& publicKeyHex, const std::string& signatureHex) {
    std::vector<uint8_t> pk;
    if (!from_hex(publicKeyHex, pk) || pk.size() != ED25519_PUBLIC_KEY_SIZE) return false;

    std::vector<uint8_t> sig;
    if (!from_hex(signatureHex, sig) || sig.size() != ED25519_SIGNATURE_SIZE) return false;

    return ed25519_verify_detached(reinterpret_cast<const uint8_t*>(message.data()), message.size(), pk.data(), sig.data());
}

} // namespace kick::crypto
