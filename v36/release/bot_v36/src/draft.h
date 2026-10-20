// Draft logic: pick champions in a fixed priority order, skipping ones we already own.
#pragma once
#include <array>
#include <cstdint>

void play_draft();

// Legacy descriptor classifier. Retained for disabled-switch parity only.
int nova_map_id(int w, int h, unsigned size, int groups);
// Exact immutable physical layout, absolute engine orientation; -1 on unavailable
// facts, changed layouts, unverified transforms, or when strict recognition is off.
// IDs use the same 0..16 order as nova_map_id. No shared/draft-memory writes.
int nova_official_map_id(int w, int h, unsigned size, int groups);

// Agent3 owns the only draft-memory assignment. This helper fills extension6..15
// after an exact draft-phase validation, preserving the existing prefix0..5.
// On false the capacity>=16 writer must submit its full zero-initialized
// extension to invalidate any previous record; SDK short writes retain tails.
bool nova_record_map_cache(std::array<std::uint8_t, 16>& bytes, unsigned actual_picks);
