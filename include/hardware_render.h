#ifndef TI60_HARDWARE_RENDER_H
#define TI60_HARDWARE_RENDER_H
#include "render.h"
#include "backend_control.h"
#include "gfx_g2d_job.h"

#define GAME_CANVAS_WIDTH 640u
#define GAME_CANVAS_HEIGHT 480u
#define GAME_CANVAS_BYTES (GAME_CANVAS_WIDTH*GAME_CANVAS_HEIGHT*2u)
/* Sum of align64(align16(source_width*2)*height), for 8 sprites/3 atlases. */
#define GAME_DMA_STORAGE_BYTES 252672u
#define GAME_DMA_ASSET_COUNT (GAME_RENDER_SOURCES+1u)

typedef struct {
    gfx_g2d_asset assets[GAME_DMA_ASSET_COUNT];
    gfx_image originals[GAME_RENDER_SOURCES];
    gfx_image canvas;
    size_t packed_bytes;
    int initialized;
} GameHardwareAssets;
/* Explicit owned CPU/device mappings. Sources are uploaded only once, before
 * any device borrowing; whole rows and DMA/cache-line padding are reserved.
 * Both renderers then consume the same padded source views and pixel values. */
gfx_error game_hardware_assets_init(GameHardwareAssets *,gfx_g2d_memory sprites,
                                    gfx_g2d_memory framebuffer);
/* Build the original 400x300 scene centered at (120,90) on a 640x480 canvas.
 * The first Fill covers the full hardware frame. No image scaling. */
gfx_error game_hardware_commands(const Game *,const GameHardwareAssets *,GameDrawList *);
/* All-or-nothing target conversion; coordinates/sources/order stay identical. */
gfx_error game_hardware_target(GameDrawList *,const GameHardwareAssets *,gfx_destination_kind);

typedef enum { GAME_HW_IDLE,GAME_HW_CPU_PRESENT,GAME_HW_NATIVE,GAME_HW_FAULT } GameHardwarePhase;
typedef struct {
    GameCpuRenderer cpu;
    gfx_g2d_context *g2d;
    gfx_job *job;
    const GameHardwareAssets *assets;
    GameHardwarePhase phase;
    GameBackend active;
    gfx_error error;
    uint64_t cpu_draw_ticks;
    gfx_g2d_progress last_native,last_display;
    uint32_t frames_started,frames_completed,presented,compare_pairs;
    size_t peak_commands,g2d_workspace_bytes;
} GameHardwareRenderer;
/* Persistent zero-initialized renderer/storage, a probed exclusive device,
 * real publication callbacks and explicit allocations are required. */
gfx_error game_hardware_renderer_init(GameHardwareRenderer *,const GameHardwareAssets *,
    void *cpu_storage,size_t cpu_bytes,void *g2d_storage,size_t g2d_bytes,
    const gfx_g2d_config *);
/* Nonblocking frame service: modes 1 CPU+G2D transport, 2 native G2D, 3 CPU then
 * G2D from the very same frozen command list. Game logic may advance meanwhile.
 * A mode request is only applied at the next IDLE start. Failure holds resources. */
gfx_error game_hardware_start(GameHardwareRenderer *,const Game *,GameBackend);
gfx_error game_hardware_poll(GameHardwareRenderer *);
#endif
