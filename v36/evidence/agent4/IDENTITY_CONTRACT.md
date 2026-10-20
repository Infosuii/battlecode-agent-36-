# Canonical physical-map identity

`draft.h`: `nova_official_map_id(int w, int h, unsigned actual_draft_picks, int group_count)` returns canonical0..16 or -1. It is a strict physical identity query when `NOVA_STRICT_MAP_ID=1`, otherwise -1. Callers use the preexisting `nova_map_id` only with the switch off, preserving disabled behavior. Failures are cached as unknown for the module's match lifetime; call only with reliable actual pick count, never C&I body count.

IDs: ambush0, collision1, contest2, corners3, eight4, elkring5, expanse6, fragment7, interlock8, lanes9, monolith10, packs11, pilgrimage12, rows13, rushbait-v2 14, stampede15, swirl16.

Physical equality includes ordered terrain-mask words (walls/bushes/holes/altars), dimensions, chunk format, actual draft picks, engine symmetry enum, absolute A spawn bounds, and group-order-independent sorted camps/types/coordinates plus gems/gold/respawn. Padding is compared exactly and masks use the installed SDK/engine layout. There is no digest collision risk in the comparison. Cosmetic colors and action history are excluded; no alternative physical layouts are recognized by descriptors alone. Original artifacts and fresh baseline replay digests are retained in the layout manifest.

Canonicalization does not accept arbitrary transformations: absolute engine coordinates are preserved. A transposed dimension descriptor may match the legacy classifier but cannot pass strict width/height and layout equality. An intrinsic map symmetry producing identical physical layout is naturally equal. A metadata/header/API failure, near-match terrain, changed rewards/camps, unsupported pick count or changed pool becomes unknown. Unknown draft choices retain spread lineup2 or dangerous-cluster lineup6. Legacy nine-map brain profile indices remain a separate mapping gated by strict recognition, never indexed directly by0..16.

Draft memory remains magic0xA5, then team size/caster count/class/max-group/flags at existing offsets; runtime shared memory is untouched. Fixtures may not repurpose memory. Allnew-switches-off calls and policy precedence retain the baseline behavior. No classifier policy gain is claimed.
