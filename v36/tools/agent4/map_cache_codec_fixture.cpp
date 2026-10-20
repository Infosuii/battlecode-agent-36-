#include "draft_map_cache.h"
#include <cstdio>

int main() {
    unsigned records = 0, bit_flips = 0, truncations = 0, contexts = 0;
    const auto fail = [](const char* reason) { std::printf("FAIL %s\n", reason); return 1; };
    for (unsigned picks = 3; picks <= 5; ++picks) {
        for (int id = -1; id <= 16; ++id) {
            std::array<std::uint8_t, 16> bytes{0xA5, static_cast<std::uint8_t>(picks), 2, 3, 255, 3};
            const auto prefix = bytes;
            if (!nova_cache::encode(bytes, 50, 65, picks, 300, id)) return fail("valid encode");
            for (int k = 0; k < 6; ++k) if (bytes[k] != prefix[k]) return fail("prefix changed");
            if (nova_cache::decode(bytes, 50, 65, picks, 300) != id) return fail("roundtrip");
            ++records;
            for (unsigned byte = 0; byte < bytes.size(); ++byte) {
                for (unsigned bit = 0; bit < 8; ++bit) {
                    auto changed = bytes;
                    changed[byte] ^= static_cast<std::uint8_t>(1u << bit);
                    if (nova_cache::decode(changed, 50, 65, picks, 300) != -2) return fail("single bit corruption accepted");
                    ++bit_flips;
                }
            }
            for (unsigned length = 0; length < 16; ++length) {
                if (nova_cache::decode({bytes.data(), length}, 50, 65, picks, 300) != -2) return fail("truncated record accepted");
                ++truncations;
            }
            if (nova_cache::decode(bytes, 51, 65, picks, 300) != -2 ||
                nova_cache::decode(bytes, 50, 66, picks, 300) != -2 ||
                nova_cache::decode(bytes, 50, 65, picks == 3 ? 4 : 3, 300) != -2 ||
                nova_cache::decode(bytes, 50, 65, picks, 301) != -2) return fail("wrong live context accepted");
            contexts += 4;
            auto unsupported = bytes;
            unsupported[7] = 2;
            unsupported[14] = nova_cache::checksum({unsupported.data(), 14});
            if (nova_cache::decode(unsupported, 50, 65, picks, 300) != -2) return fail("unsupported version with valid CRC accepted");
            auto invalid = bytes;
            if (nova_cache::encode(invalid, 256, 65, picks, 300, id) || invalid != bytes) return fail("invalid context mutated record");
            if (nova_cache::encode(invalid, 50, 65, picks, 300, 17) || invalid != bytes) return fail("invalid ID mutated record");
        }
    }
    std::printf("{\"records\":%u,\"single_bit_flips_rejected\":%u,\"truncations_rejected\":%u,\"live_context_mismatches_rejected\":%u,\"passed\":true}\n", records, bit_flips, truncations, contexts);
    return 0;
}
