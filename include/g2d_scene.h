#ifndef TI60_DANMAKU_G2D_SCENE_H
#define TI60_DANMAKU_G2D_SCENE_H

#include <stdint.h>
#include "assets.h"
#include "game.h"

/* Game-layer operation identifiers. The board runtime owns packet encoding. */
enum {
    G2D_OP_FILL = 1u,
    G2D_OP_COPY = 2u,
    G2D_OP_BEGIN_FRAME = 3u,
    G2D_OP_END_FRAME = 4u
};

typedef struct {
    uint8_t opcode;
    uint8_t alpha;
    uint16_t color_key;
    uint32_t frame_id;
    const GameSprite *source;
    int16_t x, y;
    uint16_t width, height;
    uint16_t color;
} GameG2dCommand;

typedef int (*game_g2d_submit_fn)(void *context,
                                  const GameG2dCommand *command);

typedef struct {
    void *context;
    game_g2d_submit_fn submit;
} GameG2dSink;

/* Generates one complete scene. The sink handles runtime submission and
 * completion; this function only describes game geometry and sprite effects. */
int game_render_g2d(const Game *game, uint32_t frame_id,
                    const GameG2dSink *sink);

#endif
