#include "g2d_scene.h"

#define RGB565(r, g, b) \
    (uint16_t)((((uint16_t)(r) >> 3) << 11) | \
               (((uint16_t)(g) >> 2) << 5) | ((uint16_t)(b) >> 3))

static GameG2dCommand command(uint8_t opcode, uint32_t frame_id)
{
    GameG2dCommand result = {0};
    result.opcode = opcode;
    result.alpha = 255u;
    result.color_key = GAME_COLOR_KEY;
    result.frame_id = frame_id;
    return result;
}

static int submit(const GameG2dSink *sink, GameG2dCommand command_value)
{
    if (!sink || !sink->submit) return -1;
    return sink->submit(sink->context, &command_value);
}

static int fill(const GameG2dSink *sink, uint32_t frame_id,
                int x, int y, unsigned width, unsigned height,
                uint16_t color)
{
    GameG2dCommand result = command(G2D_OP_FILL, frame_id);
    result.x = (int16_t)x;
    result.y = (int16_t)y;
    result.width = (uint16_t)width;
    result.height = (uint16_t)height;
    result.color = color;
    return submit(sink, result);
}

static int copy_sprite(const GameG2dSink *sink, uint32_t frame_id,
                       const GameSprite *sprite, int x, int y)
{
    GameG2dCommand result;
    if (!sprite) return -1;
    /* The game renderer clips partial sprites. The scene ABI has no portable
     * source-subrectangle field, so omit a partially visible COPY rather than
     * submitting negative or out-of-bounds geometry to the runtime. */
    if (x < 0 || y < 0 || x + (int)sprite->width > GAME_WIDTH ||
        y + (int)sprite->height > GAME_HEIGHT) return 0;
    result = command(G2D_OP_COPY, frame_id);
    result.source = sprite;
    result.x = (int16_t)x;
    result.y = (int16_t)y;
    result.width = sprite->width;
    result.height = sprite->height;
    return submit(sink, result);
}

static int copy_centered(const GameG2dSink *sink, uint32_t frame_id,
                         const GameSprite *sprite, int32_t x_q8,
                         int32_t y_q8)
{
    return copy_sprite(sink, frame_id, sprite,
                       (int)(x_q8 / GAME_Q) - (int)sprite->width / 2,
                       (int)(y_q8 / GAME_Q) - (int)sprite->height / 2);
}

int game_render_g2d(const Game *game, uint32_t frame_id,
                    const GameG2dSink *sink)
{
    unsigned i;
    int result;
    const GameSprite *player_sprite;
    const uint16_t playfield = RGB565(5, 7, 18);
    const uint16_t panel = RGB565(14, 18, 35);
    const uint16_t hud = RGB565(100, 100, 120);
    const uint16_t hp = RGB565(238, 55, 125);

    if (!game || !sink || !sink->submit) return -1;
    result = submit(sink, command(G2D_OP_BEGIN_FRAME, frame_id));
    if (result) return result;
    result = fill(sink, frame_id, 0, 0, GAME_WIDTH, GAME_HEIGHT, playfield);
    if (result) return result;
    result = copy_sprite(sink, frame_id, &asset_background, 0, 0);
    if (result) return result;
    result = fill(sink, frame_id, 284, 0, 116, GAME_HEIGHT, panel);
    if (result) return result;

    player_sprite = &asset_player[(game->prev_keys & GKEY_LEFT) ? 1u :
                                  (game->prev_keys & GKEY_RIGHT) ? 2u : 0u];
    result = copy_centered(sink, frame_id, &asset_boss,
                           game->boss_x, game->boss_y);
    if (result) return result;
    result = copy_centered(sink, frame_id, player_sprite,
                           game->player_x, game->player_y);
    if (result) return result;
    for (i = 0; i < GAME_PLAYER_CAP; ++i) {
        if (!game->player[i].active) continue;
        result = copy_centered(sink, frame_id, &asset_shot,
                               game->player[i].x, game->player[i].y);
        if (result) return result;
    }
    for (i = 0; i < GAME_ENEMY_CAP; ++i) {
        const GameSprite *sprite;
        if (!game->enemy[i].active) continue;
        sprite = game->enemy[i].kind ? &asset_bullet_sweep :
                                       &asset_bullet_ring;
        result = copy_centered(sink, frame_id, sprite,
                               game->enemy[i].x, game->enemy[i].y);
        if (result) return result;
    }

    result = fill(sink, frame_id, 292, 10, 88, 3, hud);
    if (result) return result;
    result = fill(sink, frame_id, 292, 10,
                  (unsigned)((game->hp * 88u) / GAME_BOSS_HP), 3, hp);
    if (result) return result;
    return submit(sink, command(G2D_OP_END_FRAME, frame_id));
}
