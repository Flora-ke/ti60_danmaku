#ifndef TI60_BACKEND_CONTROL_H
#define TI60_BACKEND_CONTROL_H
#include <stdint.h>

/* Display/render selection belongs to the platform, separate from GameMode,
 * difficulty and bullet intensity. These bits extend the existing UART bitmap. */
#define GKEY_BACKEND_CPU (1u << 10)
#define GKEY_BACKEND_G2D (1u << 11)
#define GKEY_BACKEND_COMPARE (1u << 12)
#define GAME_BACKEND_KEY_MASK (GKEY_BACKEND_CPU | GKEY_BACKEND_G2D | GKEY_BACKEND_COMPARE)
#define GAME_INPUT_MASK 0x1fffu

typedef enum GameBackend {
    GAME_BACKEND_CPU = 1, GAME_BACKEND_G2D = 2, GAME_BACKEND_COMPARE = 3
} GameBackend;

/* A single backend key rising edge selects a mode. Holding/repeating a key,
 * multiple simultaneous backend keys, and other game keys preserve current.
 * The caller applies the selected mode only after any current frame/job ends;
 * requesting a mode does not cancel or release a rendering task. */
GameBackend game_backend_select(GameBackend current, uint32_t keys,
                                uint32_t previous_keys);
#endif
