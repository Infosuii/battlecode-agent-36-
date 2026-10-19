// Exact frozen scoring body for all-new-switches-off code generation.

    PROF(score);
    const float dps = my_dps(nullptr);
    int best = -1;
    best_need = 1;
    best_score = 0;
    // Visit groups nearest-first and stop scoring a group as soon as even a free kill could not beat
    // the best score so far (branch and bound): most groups are pruned after one distance lookup.
    // Buffers are left uninitialised on purpose: filling memory costs 1 fuel per byte.
    constexpr int kMaxOrder = 256;
    struct Entry {
        int first, second;
    };
    Entry order[kMaxOrder]; // trivial type: no per-element construction cost
    int n_order = 0;
    {
    PROF(s_order);
    // Distances: one tile index for our position, then one load per group from its static field.
    const std::size_t here_idx = field_index(pos);
    const bool have_ptrs = w.field_ptr.size() == w.groups.size();
    // Visit groups in last turn's distance order: we move a step or two per turn, so the list is
    // nearly sorted and an insertion sort is close to free (the old counting sort cost ~4k fuel).
    if (group_order.size() != w.groups.size()) {
        group_order.resize(w.groups.size());
        for (std::size_t gi = 0; gi < w.groups.size(); ++gi) group_order[gi] = static_cast<int>(gi);
    }
    for (int gi : group_order) {
        if (n_order >= kMaxOrder) break;
        if (avoid_until[static_cast<std::size_t>(gi)] > round) continue;
        int dist = kUnreach;
        const std::uint16_t* f = have_ptrs ? w.field_ptr[static_cast<std::size_t>(gi)] : nullptr;
        if (f)
            dist = f[here_idx];
        else
            // Field not built yet (first turns only): straight-line distance is a lower bound.
            for (int ci : w.groups[static_cast<std::size_t>(gi)].camps) dist = std::min<int>(dist, cheb(w.camps[ci].pos, pos));
        if (dist == kUnreach) continue;
        int i = n_order++;
        for (; i > 0 && order[i - 1].first > dist; --i) order[i] = order[i - 1];
        order[i] = {dist, gi};
    }
    // Remember the order for next turn (groups skipped this turn keep their place at the end).
    {
        int k = 0;
        for (int i = 0; i < n_order; ++i) group_order[static_cast<std::size_t>(k++)] = order[i].second;
        if (k < static_cast<int>(group_order.size())) {
            // Append the groups not visited this turn, in their old relative order.
            std::vector<char>& seen = order_seen;
            seen.assign(w.groups.size(), 0);
            for (int i = 0; i < n_order; ++i) seen[static_cast<std::size_t>(order[i].second)] = 1;
            for (std::size_t gi = 0; gi < w.groups.size(); ++gi)
                if (!seen[gi]) group_order[static_cast<std::size_t>(k++)] = static_cast<int>(gi);
        }
    }
    }
    // Score the current target first: its (sticky) score is a strong bound.
    for (int oi = 1; oi < n_order; ++oi)
        if (order[oi].second == target_group) {
            std::swap(order[0], order[oi]);
            break;
        }
    // Route search keeps the K best first targets (pruning against the K-th best score).
    const int keep = tune::kRouteDepth >= 2 ? std::clamp(tune::kRouteK, 1, 8) : 1;
    RouteCand top[8];
    int n_top = 0;
    // With the route search, groups within kRouteSlack of the best single score stay candidates.
    auto prune_bound = [&] {
        if (keep == 1) return best_score;
        const float kth = n_top < keep ? 0.0f : top[n_top - 1].score;
        return std::max(kth, best_score * tune::kRouteSlack);
    };
    if (NOVA_BIGJOB > 0.0f && big_group.size() != w.groups.size()) {
        // Static per map: total gems over all groups, and which groups hold a dominant share of them.
        float total = 0.0f;
        for (const auto& g2 : w.groups) total += static_cast<float>(g2.gems);
        big_group.assign(w.groups.size(), 0);
        for (std::size_t k = 0; k < w.groups.size(); ++k)
            big_group[k] = total > 0.0f && w.groups[k].gems > 0 && static_cast<float>(w.groups[k].gems) >= NOVA_BIGJOB * total;
    }
    int evaluated = 0; // groups past the bound (APEX_DET caps them)
    for (int oi = 0; oi < n_order; ++oi) {
        // Out of fuel (huge maps, first turn): settle for the best group scored so far.
        if (tune::kDet) {
            if (best >= 0 && (evaluated >= tune::kDetScoreN || bc::fuel_remaining() < static_cast<std::uint64_t>(tune::kDetSafety))) break;
        } else if ((oi & 3) == 3 && best >= 0 && bc::fuel_remaining() < tune::kScoreFloor)
            break;
        const int dist = order[oi].first;
        const std::size_t gi = static_cast<std::size_t>(order[oi].second);
        auto& g = w.groups[gi];
        const int approach = std::max(0, dist - (g.state == GroupState::reward ? 0 : range));
        int wait = 0;
        if (g.state == GroupState::respawning)
            wait = static_cast<int>(static_cast<float>(std::max(0, g.respawn_round - round - approach)) * tune::kWaitWeight);
        if (NOVA_BIGCAMP > 0 && g.gems >= 120)
            wait = static_cast<int>(wait * (NOVA_BIGCAMP * 0.01f)); // big strips respawn on a cycle: cheap waiting
        {
            const float best_case = (static_cast<float>(g.gems) + gold_weight() * static_cast<float>(g.gold)) *
                tune::kStickiness / (static_cast<float>(approach + wait) + tune::kCaptureTime + 4.0f);
            if (best_case <= prune_bound()) {
#ifdef APEX_SCOREDBG
                if (g.harmless && g.gems > 0 && round < 60)
                    std::printf("SDBG R%d slot%d G%zu pruned d%d st%d best_case %.3f bound %.3f\n", round, slot, gi, dist,
                        static_cast<int>(g.state), best_case, prune_bound());
#endif
                continue;
            }
        }
        ++evaluated;
        if (group_capture_covered(g)) {
            avoid_until[gi] = round + 6; // an ally is already capturing it
            continue;
        }
        if ((NOVA_DUO & 8) && duo_ignore_slot >= 0 && g.state == GroupState::reward && !g.camps.empty() && w.enemies.empty()) {
            // NOVA_DUO bit 8: our ranged follower takes the capture; we walk on and tank the next camp.
            bool follower_near = false;
            for (const auto& a : w.allies)
                if (a.id == ally_handle[static_cast<std::size_t>(duo_ignore_slot)] && cheb(a.pos, w.camps[g.camps[0]].pos) <= 4)
                    follower_near = true;
            if (follower_near) {
                avoid_until[gi] = round + 3;
                continue;
            }
        }
        if (tune::kShareGold && g.state == GroupState::reward && w.enemies.empty() &&
            poorer_ally_takes(g, static_cast<int>(gi), false)) {
            avoid_until[gi] = round + 4; // a poorer ally takes this reward
            continue;
        }
        int lower = 0, higher = 0;
        {
            PROF(s_claims);
            std::tie(lower, higher) = claims_split(static_cast<int>(gi));
        }
        const int claims = lower + higher;
        const float hp_left = g.hp_left(w.camps);
        int need = 1;
        {
            PROF(s_need);
            need = champions_needed(g, hp_left);
        }
        // NOVA_BIGJOB: the soak model caps at 7 and kept stampede's 17k-health gem camp "impossible" until round 250;
        // opponents with our lineup take it near round 150. A dominant camp is a whole-team job.
        if (NOVA_BIGJOB > 0.0f && gi < big_group.size() && big_group[gi] && need > team_size) need = team_size;
        if (NOVA_BIGJOB > 0.0f && gi < big_group.size() && big_group[gi] && round % 25 == 0)
            LOG("BIG R%d slot%d G%zu need%d team%d claims%d hp_left%.0f state%d dist%d\n", round, slot, gi, need, team_size,
                claims, hp_left, static_cast<int>(g.state), dist);
        if (need > team_size) continue;
        // A job that needs more champions than are on it: open it as a call for help (allies see the
        // claim and join), but only if no earlier slot already has an open call we could answer.
        bool initiating = need > std::max(claims + 1, participants_hint);
        // Siege: spells reach 4 tiles and camps only 3, so casters can wear any camp down safely.
        const float sdps = initiating && tune::kSiege && !in_role_pack ? siege_dps(g) : 0.0f;
        const bool siege = sdps > 0.0f;
        if (siege) initiating = false;
        else if (initiating) {
            const bool big = NOVA_BIGJOB > 0.0f && gi < big_group.size() && big_group[gi];
            if (!tune::kInitiateJobs || no_initiate || round < call_ban_until || need > (big ? team_size : 2) || open_call_by_lower())
                continue;
        }
        const bool solo_job = need == 1 && (g.state == GroupState::reward || hp_left < 1500.0f);
        // Staffing cap (APEX_CAP_STAFF = n): a group that earlier-acting allies already staff to its
        // need (plus n - 1 spare) is theirs. Without it whole teams piled onto one wolf group, or
        // waited two at a time beside one respawning group, while the next group sat untouched.
        if (tune::kCapStaff > 0 && !solo_job && g.state != GroupState::reward && lower >= need + tune::kCapStaff - 1) continue;
        // Small jobs belong to the earliest-acting claimant; later slots leave them alone.
        // (Everyone reacting to everyone else's claims made whole teams flip targets each turn.)
        const int helpers = solo_job ? 0 : claims;
        const int participants = siege ? 1 : std::max({need, helpers + 1, participants_hint});
        const float kill = siege ? hp_left / sdps : hp_left / (my_dps(&g) + static_cast<float>(participants - 1) * dps);
        float value = static_cast<float>(g.gems) + gold_weight() * static_cast<float>(g.gold);
        if (APEX_BOOT_ITEMS > 0 && g.harmless && my_items() < static_cast<float>(APEX_BOOT_ITEMS)) value *= 1.0f + tune::kBootBonus;
        // (With gold sharing, finished groups ignore claims: the poorest champion there takes them.)
        if (solo_job && !(tune::kShareGold && g.state == GroupState::reward) &&
            (tune::kIncumbent ? senior_claims(static_cast<int>(gi)) : lower) > 0)
            value *= 0.15f;
        if (round - g.enemy_round < 15 && !(NOVA_BIGJOB > 0.0f && gi < big_group.size() && big_group[gi])) value *= 0.6f;
        if (NOVA_BIGJOB > 0.0f && round >= NOVA_BIGJOB_FROM && gi < big_group.size() && big_group[gi]) value *= NOVA_BIGJOB_MULT;
        // Altar-side groups (APEX_ALTAR_BONUS): their gems bank at once, without a trip.
        if (tune::kAltarBonus > 0.0f && w.altar_ready && !g.camps.empty() && fdist(w.altar_field, w.camps[g.camps[0]].pos) <= 6)
            value *= 1.0f + tune::kAltarBonus;
        // Numbers (APEX_NUMBERS_INVADE): with enough extra living champions, take the enemy side's groups.
        if (alive_us - alive_them >= tune::kNumbersInvade && w.enemy_ready && w.group_margin[gi] < 0) value *= 1.0f + tune::kNumbersBonus;
        // Territory (APEX_TERRITORY, until round APEX_TERRITORY_UNTIL): our own side first. The evaluator puts
        // early contact at -13 gems per champion; c1 spent 63% of its time on the enemy half against 46% for
        // the top teams and earned 30% less in the first 150 rounds.
        if (tune::kTerritory < 1.0f && round < tune::kTerritoryUntil && w.enemy_ready && w.group_margin[gi] < -tune::kTerritoryMargin)
            value *= tune::kTerritory;
        // Cohesion (APEX_COHESION): a camp far from every teammate gives up the safety of numbers. The top
        // teams operate 7.5 tiles apart against our 11.3 and die a third as often (540 decoded ladder games):
        // value falls by APEX_COHESION per tile beyond APEX_COHESION_DIST from the nearest living teammate.
        if (tune::kCohesion > 0.0f && !w.allies.empty() && !g.camps.empty()) {
            const vec2 gp = w.camps[g.camps[0]].pos;
            int dmin = 999;
            for (const auto& a : w.allies) dmin = std::min(dmin, cheb(a.pos, gp));
            const int over = dmin - tune::kCohesionDist;
            if (over > 0) value /= 1.0f + tune::kCohesion * static_cast<float>(over);
        }
        // NOVA_BUDDY: farm our own group, but near our buddy's target (read from its claim slot, so it works
        // out of vision): top bots farm separate camps a few tiles apart and fight with help close by.
        if (NOVA_BUDDY > 0.0f && !g.camps.empty()) {
            const vec2 anchor = buddy_anchor();
            if (anchor.x >= 0) {
                const int over = cheb(anchor, w.camps[g.camps[0]].pos) - NOVA_BUDDY_DIST;
                if (over > 0) value /= 1.0f + NOVA_BUDDY * static_cast<float>(over);
            }
        }
        // NOVA_SEED (0 = off): a small deterministic jitter on group values so one official map can be replayed
        // as many distinct games (evaluation only; neutral on average).
        if (NOVA_SEED > 0) {
            std::uint32_t hs = static_cast<std::uint32_t>(NOVA_SEED) * 2654435761u ^ static_cast<std::uint32_t>(gi) * 40503u ^
                static_cast<std::uint32_t>(round / 20) * 2246822519u ^ static_cast<std::uint32_t>(slot + 1) * 3266489917u;
            hs ^= hs >> 15;
            hs *= 2246822519u;
            hs ^= hs >> 13;
            value *= 1.0f + NOVA_SEED_EPS * (static_cast<float>(hs & 0xffffu) / 32767.5f - 1.0f);
        }
        // Champion-turns the team spends on this group, plus the health it drains: lost health costs a
        // trip home later (APEX_HP_COST turns per full health bar), which made wolves look better
        // than harmless gem clusters.
        float drain = 0.0f;
        if (tune::kHpCost > 0.0f && !g.harmless && g.state != GroupState::reward) {
            const float team_dps = my_dps(&g) + static_cast<float>(participants - 1) * dps;
            drain = soak_coefficient(static_cast<int>(gi), hp_left) * camp_taken_mult / std::max(team_dps, 1.0f) /
                std::max(max_hp, 1.0f) * tune::kHpCost;
        }
        const float cost = static_cast<float>(participants) * (static_cast<float>(approach + wait) + kill) +
            tune::kCaptureTime + 4.0f + drain;
        float score = value / cost;
#ifdef APEX_SCOREDBG
        if (g.harmless && g.gems > 0 && round < 60)
            std::printf("SDBG R%d slot%d G%zu d%d st%d claims %d/%d value %.1f cost %.1f score %.3f best %.3f\n", round, slot, gi,
                dist, static_cast<int>(g.state), lower, higher, value, cost, score, best_score);
#endif
        if (initiating) score *= 0.7f; // uncertain help: prefer jobs we can finish now
        if (siege) score *= 0.8f;
        if (static_cast<int>(gi) == target_group) score *= tune::kStickiness; // finish what we started
        if constexpr (learner::on) {
            // NOVA_LEARNER: learned residual on the hand score. Zero weights = identity, so the
            // switch is a no-op until trained weights land in NOVA_LEARNER_W.
            const int state_ord = static_cast<int>(g.state);
            float phi[16];
            learner::gather(&g, hp_left, kill, drain, approach, wait, dist, participants, need, state_ord,
                g.harmless, lower, higher, value, score, siege, initiating, solo_job, phi);
            phi[0] = static_cast<float>(g.gems) / 64.0f > 3.0f ? 3.0f : static_cast<float>(g.gems) / 64.0f;
            phi[1] = static_cast<float>(g.gold) / 64.0f > 3.0f ? 3.0f : static_cast<float>(g.gold) / 64.0f;
            score = learner::fix(score, phi);
        }
        if (tune::kBelief && tune::kBeliefW > 0.0f && score > prune_bound() && !g.camps.empty() && bc::fuel_remaining() > (tune::kDet ? static_cast<std::uint64_t>(tune::kDetSafety) : 30000)) {
            // Enemies that could be at this group before we are done with it (beliefs), beyond what our side
            // brings: contested jobs get champions caught alone.
            const int finish = approach + wait + static_cast<int>(kill);
            const vec2 gp = w.camps[g.camps[0]].pos;
            const float e = w.expected_enemies(gp, std::min(finish, 12) + 2);
            int friends = 1;
            for (const auto& a : w.allies) friends += cheb(a.pos, gp) <= 6;
            const float excess = e - tune::kBeliefAlly * static_cast<float>(friends);
            if (excess > 0.0f) score /= 1.0f + tune::kBeliefW * excess;
        }
        if (score > best_score) {
            best_score = score;
            best = static_cast<int>(gi);
            best_need = need;
        }
        if (keep > 1 && (n_top < keep || score > top[n_top - 1].score)) {
            // Turns until this champion is free again: walk, wait, kill, capture.
            const int finish = approach + wait + static_cast<int>(kill) + tune::kCaptureTime;
            int i = n_top < keep ? n_top++ : keep - 1;
            for (; i > 0 && top[i - 1].score < score; --i) top[i] = top[i - 1];
            top[i] = {static_cast<int>(gi), need, value, cost, score, finish};
        }
    }
    if (keep > 1 && n_top >= 2 && bc::fuel_remaining() > tune::kRouteFuel) {
        bool bank = false;
        const int pick = tune::kPlan ? plan_search(top, n_top, bank) : route_search(top, n_top);
        if (tune::kPlan && bank) plan_bank_round = round;
        if (pick >= 0) {
            best = top[pick].gi;
            best_need = top[pick].need;
            best_score = top[pick].score;
        }
    }
    return best;
