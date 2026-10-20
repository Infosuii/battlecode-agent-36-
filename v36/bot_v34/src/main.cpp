// Entry point: dispatch draft and gameplay turns; errors are logged and the turn still ends.
#include <iostream>

#include "bot.h"
#include "draft.h"

int main() {
    Bot bot;
    while (true) {
        try {
            if (bc::is_draft())
                play_draft();
            else
                bot.play_turn();
        } catch (const std::exception& error) {
            std::printf("bot error: %s\n", error.what());
        } catch (...) {
            std::printf("bot error: unknown exception\n");
        }
        bc::end_turn();
    }
}
