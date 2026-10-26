#include "backend_control.h"

GameBackend game_backend_select(GameBackend current, uint32_t keys,
                                uint32_t previous_keys)
{
    uint32_t requested = keys & GAME_BACKEND_KEY_MASK;
    if (!requested || (requested & (requested - 1u)) ||
        !(requested & ~previous_keys)) return current;
    if (requested == GKEY_BACKEND_CPU) return GAME_BACKEND_CPU;
    if (requested == GKEY_BACKEND_G2D) return GAME_BACKEND_G2D;
    return GAME_BACKEND_COMPARE;
}
