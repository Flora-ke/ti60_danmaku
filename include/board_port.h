#ifndef TI60_BOARD_PORT_H
#define TI60_BOARD_PORT_H
#include <stddef.h>
#include <stdint.h>
#include "g2d_scene.h"

/* UART: A5 5A 01 03 sequence keys_lo keys_hi CRC8(version..keys_hi).
 * State snapshots, no host structs, pointers or native byte order on the wire. */
#define BOARD_UART_FRAME_SIZE 8u
#define BOARD_UART_BYTE_TIMEOUT_MS 100u
#define BOARD_UART_RELEASE_MS 250u
typedef struct {
    uint8_t bytes[BOARD_UART_FRAME_SIZE];
    uint8_t used;
    uint16_t keys, allowed_keys;
    uint32_t last_byte_ms, last_valid_ms, accepted, rejected;
} BoardInput;
void board_input_init(BoardInput *input, uint16_t allowed_keys);
void board_input_feed(BoardInput *input, uint8_t byte, uint32_t now_ms);
uint32_t board_input_keys(BoardInput *input, uint32_t now_ms);
uint8_t board_uart_crc8(const uint8_t *bytes, size_t count);

enum { BOARD_OK = 0, BOARD_UNAVAILABLE = -1, BOARD_ERROR = -2 };
/* Default implementation returns UNAVAILABLE. With BOARD_VIDEO_ADAPTER_EXTERNAL
 * these symbols MUST be supplied by a real display adapter at link time.
 * CPU render always uses RGB565 and stride in pixels. Adapter must confirm this
 * format or convert/scale explicitly, and own all cache/DMA synchronization.
 * init: register both buffers without retaining ownership for scanout.
 * acquire: return only an index in [0,1] which is safe for CPU writes.
 * present: enqueue completed buffer; never report OK before ownership is safe.
 * Returning an error stops main permanently, preserving buffers for inspection. */
int board_video_init(uint16_t *buffer0, uint16_t *buffer1,
                     unsigned width, unsigned height, size_t stride_pixels);
int board_video_acquire(unsigned *index);
int board_video_present(unsigned index);

/* G2D runtime boundary. Define BOARD_G2D_ADAPTER_EXTERNAL when linking the
 * real board adapter. It owns MMIO encoding, source-address translation,
 * queue backpressure, cache maintenance, completion and frame ownership. */
int board_g2d_init(void);
int board_g2d_acquire(uint32_t *frame_id);
int board_g2d_submit(void *context, const GameG2dCommand *command);
int board_g2d_wait(uint32_t frame_id);
int board_g2d_present(uint32_t frame_id);
#endif
