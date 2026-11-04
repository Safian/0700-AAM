// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <filesystem>

namespace kick::crypto {

// Hex utilities
std::string to_hex(const uint8_t* data, size_t len);
std::string to_hex(const std::vector<uint8_t>& data);
bool from_hex(const std::string& hex, std::vector<uint8_t>& out);

// SHA-256 (lowercase 64-character hex string)
std::string sha256_buffer(const void* data, size_t len);
std::string sha256_string(const std::string& str);
std::string sha256_file(const std::filesystem::path& path);

// Ed25519 signatures (detached, 64-byte signature)
constexpr size_t ED25519_PUBLIC_KEY_SIZE = 32;
constexpr size_t ED25519_SECRET_KEY_SIZE = 64;
constexpr size_t ED25519_SIGNATURE_SIZE  = 64;

bool ed25519_create_keypair(uint8_t* outPublicKey32, uint8_t* outSecretKey64);
bool ed25519_create_keypair_from_seed(const uint8_t* seed32, uint8_t* outPublicKey32, uint8_t* outSecretKey64);

bool ed25519_sign_detached(const uint8_t* message, size_t messageLen,
                           const uint8_t* secretKey64, uint8_t* outSignature64);

bool ed25519_verify_detached(const uint8_t* message, size_t messageLen,
                             const uint8_t* publicKey32, const uint8_t* signature64);

// Convenience methods with strings / vectors
bool ed25519_sign_string(const std::string& message, const std::string& secretKeyHex, std::string& outSignatureHex);
bool ed25519_verify_string(const std::string& message, const std::string& publicKeyHex, const std::string& signatureHex);

} // namespace kick::crypto
