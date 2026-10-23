#ifndef TI60_DANMAKU_ASSETS_H
#define TI60_DANMAKU_ASSETS_H

#include <stdint.h>

/* Numeric RGB565. CPU renderer and future G2D must agree on this color key. */
#define GAME_COLOR_KEY UINT16_C(0xF81F)

typedef struct {
    uint16_t width;
    uint16_t height;
    const uint16_t *pixels;
} GameSprite;

extern const GameSprite asset_background; /* 400 x 300, opaque */
extern const GameSprite asset_player[3];  /* idle, left, right: 16 x 24 */
extern const GameSprite asset_boss;       /* 21 x 34 */
extern const GameSprite asset_bullet_ring;  /* 8 x 8 */
extern const GameSprite asset_bullet_sweep; /* 10 x 17 */
extern const GameSprite asset_shot;         /* 6 x 7 */

#endif
