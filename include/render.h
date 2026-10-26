#ifndef TI60_GAME_RENDER_H
#define TI60_GAME_RENDER_H
#include <stddef.h>
#include <stdint.h>
#include "game.h"
#include "gfx_cpu.h"
/* 576 actor copies + 32 scene/protection commands + 256 glyph copies. */
#define GAME_DRAW_CAPACITY (GAME_ENEMY_CAP + GAME_PLAYER_CAP + 32u + 256u)
#define GAME_RENDER_SOURCES 11u
/* Original image views, including the three glyph atlases, in stable registry
 * order. The hardware adapter pads/copies these once, without changing pixels. */
gfx_error game_render_source(size_t index, gfx_image *out);
typedef struct {
    gfx_command commands[GAME_DRAW_CAPACITY];
    size_t count, capacity;
    gfx_error result;
} GameDrawList;
/* Metadata only, never writes destination pixels. On failure count=0 (alias
 * rejection preserves the list unchanged, so check the returned error). Target
 * must be 400x300 RGB565_LE; stride/bytes are bytes, not pixels. List/game/
 * target storage must not overlap. Assets and buffers live through job release. */
void game_draw_list_init(GameDrawList *list, size_t capacity);
gfx_error game_build_commands(const Game *game, const gfx_image *destination,
                             GameDrawList *list);
typedef struct {
    GameDrawList frame;
    gfx_cpu_context *cpu;
    gfx_job *job;
    size_t workspace_required, workspace_reserved, peak_commands;
    uint32_t submitted_frames, completed_frames;
} GameCpuRenderer;
/* Initialize unused renderer once with aligned, caller-owned persistent storage.
 * Shared CPU context performs zero per-frame allocations. Reinitialization is
 * not a cancellation mechanism; finish/release pending jobs before reuse. */
gfx_error game_cpu_renderer_init(GameCpuRenderer *renderer, void *storage,
                                 size_t storage_bytes);
gfx_error game_cpu_finish(GameCpuRenderer *renderer, uint64_t timeout_ticks);
gfx_error game_cpu_execute(GameCpuRenderer *renderer, const GameDrawList *list,
                           uint64_t timeout_ticks);
/* This convenience entry accepts pixel counts; converts them to gfx bytes.
 * Pending/timeout retains job and buffers, and refuses subsequent frames. */
gfx_error game_render_cpu(GameCpuRenderer *renderer, const Game *game,
                          uint16_t *pixels, size_t stride_pixels,
                          size_t storage_pixels, uint64_t timeout_ticks);
#endif
