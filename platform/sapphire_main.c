/* Actual newSoC Sapphire BSP entry. This is CPU rendering, not G2D. */
#include "game.h"
#include "render.h"
#include "board_port.h"
#include "bsp.h"

#if BSP_CLINT_HZ < 1000
#error BSP_CLINT_HZ must describe the actual monotonic timer frequency
#endif

static Game game;
static BoardInput input;
/* Linker must reserve these in initialized DDR, never the default 124 KiB map.
 * Separate rows preserve 64-byte cache-line alignment of both buffers. */
static uint16_t frames[2][GAME_WIDTH * GAME_HEIGHT] __attribute__((aligned(64)));
volatile int board_game_error;
volatile unsigned board_uart_timeout;

static uint64_t now_ticks(void) { return clint_getTime(BSP_CLINT); }
static uint32_t now_ms(uint64_t ticks) { return (uint32_t)(ticks / (BSP_CLINT_HZ / 1000u)); }

static int put_char(char c)
{
    unsigned tries;
    for (tries = 0; tries < 100000u; ++tries) {
        if (uart_writeAvailability(BSP_UART_TERMINAL)) {
            write_u32((uint32_t)(uint8_t)c, BSP_UART_TERMINAL + UART_DATA);
            return 1;
        }
    }
    board_uart_timeout = 1;
    return 0;
}
static void log_text(const char *text)
{
    if (board_uart_timeout) return;
    while (*text) if (!put_char(*text++)) return;
}
static void log_hex(uint32_t value)
{
    int shift;
    static const char hex[] = "0123456789abcdef";
    for (shift = 28; shift >= 0; shift -= 4)
        if (!put_char(hex[(value >> shift) & 15u])) return;
}
#ifndef RAM_ONLY_DIAGNOSTIC
static void fail_stop(int error, const char *message)
{
    board_game_error = error;
    log_text(message);
    /* Preserve buffers/state. No resetting DMA or overwriting in-flight data. */
    for (;;) { }
}
#endif
static uint32_t poll_keys(uint64_t ticks)
{
    unsigned budget;
    uint32_t ms = now_ms(ticks);
    /* Bounded, nonblocking, and only this consumer reads UART data. */
    for (budget = 0; budget < 64u && uart_readOccupancy(BSP_UART_TERMINAL); ++budget)
        board_input_feed(&input, (uint8_t)read_u32(BSP_UART_TERMINAL + UART_DATA), ms);
    return board_input_keys(&input, ms);
}

#ifdef RAM_ONLY_DIAGNOSTIC
static uint32_t frame_hash(const uint16_t *pixels)
{
    uint32_t hash = 2166136261u;
    size_t pixel;
    for (pixel = 0; pixel < GAME_WIDTH * GAME_HEIGHT; ++pixel) {
        hash = (hash ^ (pixels[pixel] & 255u)) * 16777619u;
        hash = (hash ^ (pixels[pixel] >> 8)) * 16777619u;
    }
    return hash;
}
#endif

int main(void)
{
    uint64_t previous, now, elapsed, logic_acc = 0, draw_acc = 0, last_log;
    uint32_t rendered = 0, late = 0, keys;
    unsigned index = 0, steps;
    int result;
    bsp_init();
    board_input_init(&input, (uint16_t)(GKEY_LEFT | GKEY_RIGHT | GKEY_UP | GKEY_DOWN |
                     GKEY_FIRE | GKEY_START | GKEY_PAUSE | GKEY_DEMO | GKEY_LEVEL));
    log_text("Ti60 Danmaku: CPU RGB565 renderer, CLINT pacing, UART snapshots.\r\n");
#ifdef RAM_ONLY_DIAGNOSTIC
    log_text("RAM_ONLY_DIAGNOSTIC: NO VIDEO OUTPUT. Frames stay in DDR.\r\n");
    result = BOARD_OK;
#else
    result = board_video_init(frames[0], frames[1], GAME_WIDTH, GAME_HEIGHT, GAME_WIDTH);
    if (result != BOARD_OK)
        fail_stop(result, "STOP: video adapter unavailable/failed; no interactive game started.\r\n");
#endif
    (void)result;
    game_init(&game, GMODE_DEMO, 256u);
    last_log = previous = now_ticks();
    for (;;) {
        now = now_ticks();
        elapsed = now - previous;
        previous = now;
        keys = poll_keys(now);
        /* Avoid unbounded catchup after debugger stalls. Simulation slows under
         * sustained overload; it never pretends the board achieved target FPS. */
        if (elapsed > BSP_CLINT_HZ / 4u) { elapsed = BSP_CLINT_HZ / 4u; ++late; }
        logic_acc += elapsed * GAME_HZ;
        draw_acc += elapsed * 30u;
        for (steps = 0; logic_acc >= BSP_CLINT_HZ && steps < 8u; ++steps) {
            game_step(&game, keys);
            logic_acc -= BSP_CLINT_HZ;
        }
        if (logic_acc >= BSP_CLINT_HZ) { logic_acc %= BSP_CLINT_HZ; ++late; }
        if (draw_acc >= BSP_CLINT_HZ) {
            draw_acc %= BSP_CLINT_HZ;
#ifdef RAM_ONLY_DIAGNOSTIC
            index ^= 1u;
#else
            result = board_video_acquire(&index);
            if (result != BOARD_OK || index >= 2u)
                fail_stop(result == BOARD_OK ? BOARD_ERROR : result,
                          "STOP: video acquire failed/invalid ownership index.\r\n");
#endif
            game_render(&game, frames[index], GAME_WIDTH);
#ifndef RAM_ONLY_DIAGNOSTIC
            result = board_video_present(index);
            if (result != BOARD_OK)
                fail_stop(result, "STOP: video present failed; state preserved.\r\n");
#endif
            ++rendered;
        }
        if (now - last_log >= BSP_CLINT_HZ) {
            last_log = now;
            /* Hex telemetry counts, cumulative. Not measured display FPS. */
            log_text("tick="); log_hex(game.tick);
            log_text(" enemy="); log_hex(game.active_enemy);
            log_text(" render_total="); log_hex(rendered);
            log_text(" late="); log_hex(late);
            log_text(" hash="); log_hex(game_hash(&game));
            log_text(" rx="); log_hex(input.accepted);
#ifdef RAM_ONLY_DIAGNOSTIC
            /* Read completed RAM pixels so telemetry proves rendering happened,
             * including with link-time optimization. This adds diagnostic cost. */
            log_text(" frame_hash="); log_hex(frame_hash(frames[index]));
#endif
            log_text("\r\n");
        }
    }
}
