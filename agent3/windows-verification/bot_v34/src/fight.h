// Skirmish simulator: plays a few rounds of a champion fight forward under two plans
// (our side engages, or our side retreats) so the bot can pick the better one.
#pragma once
#include "common.h"

struct SimUnit {
    int team = 0;          // 0 = ours, 1 = theirs
    vec2 pos;
    float hp = 0, max_hp = 1, str = 0, nuke = 0;
    int range = 1;
    bool stun = false;
    int cd = 0;            // turns until the damage spell is ready
    int spell_cd = 8;      // cooldown after a cast (4 when the kit has two damage spells)
    int stunned = 0;
    float death_cost = 0;  // value lost by its team if it dies (bag, respawn time, kill gold)
    bool alive = true;
};

struct SimResult {
    float score = 0;       // their losses minus ours (health fractions and deaths)
    int our_deaths = 0, their_deaths = 0;
};

// Champion-targeted spell damage per cast at the given focus, plus whether it stuns.
struct SpellKit {
    float nuke = 0;
    bool stun = false;
    bool two_spells = false;
};
SpellKit spell_kit(bc::champion_type type, float focus);

SimResult simulate_fight(std::vector<SimUnit> units, bool ours_retreat, int turns);
