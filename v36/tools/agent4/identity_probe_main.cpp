// Correctness fixture only: actual SDK map query/draft policy, no gameplay policy.
#include "common.h"
#include "draft.h"

int main() {
    bool game_printed = false;
    while (true) {
        try {
            const bool draft = bc::is_draft();
            if (draft || !game_printed) {
                std::uint32_t groups = 0;
                const int code = bc::detail::sdk_map_num_camp_groups(&groups);
                const auto size = bc::draft::team_size();
                const int w = bc::map::width(), h = bc::map::height();
                const int id = code == 0 ? nova_official_map_id(w, h, size, static_cast<int>(groups)) : -1;
                std::printf("IDENTITY_PROBE phase%d size%u groups%u code%d id%d\n", draft ? 1 : 0, size, groups, code, id);
                std::printf("IDENTITY_MEMORY capacity%u snapshot%zu cache%d\n", bc::constants::draft_memory_bytes, bc::draft_memory.size(), NOVA_STRICT_MAP_CACHE);
                if (!draft) game_printed = true;
            }
            if (draft) play_draft();
        } catch (const std::exception& ex) {
            std::printf("IDENTITY_ERROR %s\n", ex.what());
        } catch (...) {
            std::printf("IDENTITY_ERROR unknown\n");
        }
        bc::end_turn();
    }
}
