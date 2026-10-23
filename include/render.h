#ifndef TI60_GAME_RENDER_H
#define TI60_GAME_RENDER_H
#include <stddef.h>
#include <stdint.h>
#include "game.h"
/* CPU reference renderer. stride_pixels is in uint16_t pixels, not bytes.
 * Caller owns an entire 400x300 frame; no heap or device access occurs. */
void game_render(const Game *game, uint16_t *pixels, size_t stride_pixels);
#endif
