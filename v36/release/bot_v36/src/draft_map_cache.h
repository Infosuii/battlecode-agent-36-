// Versioned extension to the existing six-byte draft-memory prefix.
// This is accidental-corruption detection over our own immutable team memory,
// not authentication against a party able to forge an entire valid record.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace nova_cache {
inline constexpr std::size_t kBytes = 16;
inline constexpr std::uint8_t kPrefixMagic = 0xA5, kMagic = 0xD6, kVersion = 1, kEnd = 0x6B;

inline std::uint8_t checksum(std::span<const std::uint8_t> bytes) {
    std::uint8_t crc = 0;
    for (auto byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = static_cast<std::uint8_t>((crc & 0x80u) ? (crc << 1u) ^ 0x07u : crc << 1u);
    }
    return crc;
}

inline bool context_fits(int w, int h, unsigned picks, int groups) {
    return w > 0 && w <= 255 && h > 0 && h <= 255 && picks >= 3 && picks <= 5 && groups >= 0 && groups <= 65535;
}

// On failure leave every byte unchanged; on success leave prefix0..5 unchanged.
inline bool encode(std::array<std::uint8_t, kBytes>& bytes, int w, int h, unsigned picks, int groups, int id) {
    if (!context_fits(w, h, picks, groups) || id < -1 || id > 16 || bytes[0] != kPrefixMagic || bytes[1] != picks)
        return false;
    bytes[6] = kMagic;
    bytes[7] = kVersion;
    bytes[8] = static_cast<std::uint8_t>(id + 1);
    bytes[9] = static_cast<std::uint8_t>(picks);
    bytes[10] = static_cast<std::uint8_t>(w);
    bytes[11] = static_cast<std::uint8_t>(h);
    bytes[12] = static_cast<std::uint8_t>(groups & 255);
    bytes[13] = static_cast<std::uint8_t>((groups >> 8) & 255);
    bytes[14] = checksum(std::span<const std::uint8_t>{bytes.data(), 14});
    bytes[15] = kEnd;
    return true;
}

// -2 means missing/invalid; -1 means a valid cached unknown. The caller also
// verifies known IDs against the cheap descriptor for this exact context.
inline int decode(std::span<const std::uint8_t> bytes, int w, int h, unsigned picks, int groups) {
    if (bytes.size() < kBytes || !context_fits(w, h, picks, groups) || bytes[0] != kPrefixMagic || bytes[1] != picks ||
        bytes[6] != kMagic || bytes[7] != kVersion || bytes[8] > 17 || bytes[9] != picks || bytes[10] != w || bytes[11] != h ||
        (static_cast<unsigned>(bytes[12]) | (static_cast<unsigned>(bytes[13]) << 8u)) != static_cast<unsigned>(groups) ||
        bytes[14] != checksum(bytes.first(14)) || bytes[15] != kEnd) return -2;
    return static_cast<int>(bytes[8]) - 1;
}
} // namespace nova_cache
