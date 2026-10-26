#include "board_port.h"
#include <string.h>

uint8_t board_uart_crc8(const uint8_t *bytes, size_t count)
{
    uint8_t crc = 0;
    size_t i;
    unsigned bit;
    for (i = 0; i < count; ++i) {
        crc ^= bytes[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (uint8_t)((crc & 128u) ? ((unsigned)crc << 1) ^ 7u : (unsigned)crc << 1);
    }
    return crc;
}

void board_input_init(BoardInput *input, uint16_t allowed_keys)
{
    memset(input, 0, sizeof(*input));
    input->allowed_keys = allowed_keys;
}

static void resync(BoardInput *input, uint8_t byte)
{
    input->used = byte == 0xa5u ? 1u : 0u;
    input->bytes[0] = byte;
}

void board_input_feed(BoardInput *input, uint8_t byte, uint32_t now_ms)
{
    uint16_t keys;
    if (input->used && now_ms - input->last_byte_ms > BOARD_UART_BYTE_TIMEOUT_MS) {
        input->used = 0;
        ++input->rejected;
    }
    input->last_byte_ms = now_ms;
    if (!input->used) { resync(input, byte); return; }
    if (input->used == 1 && byte != 0x5au) { resync(input, byte); return; }
    input->bytes[input->used++] = byte;
    if ((input->used == 3 && byte != 1u) || (input->used == 4 && byte != 3u)) {
        ++input->rejected;
        resync(input, byte);
        return;
    }
    if (input->used != BOARD_UART_FRAME_SIZE) return;
    keys = (uint16_t)(input->bytes[5] | ((uint16_t)input->bytes[6] << 8));
    if (board_uart_crc8(input->bytes + 2, 5) == byte &&
        (keys & (uint16_t)~input->allowed_keys) == 0u) {
        input->keys = keys;
        input->last_valid_ms = now_ms;
        ++input->accepted;
        input->used = 0;
    } else {
        ++input->rejected;
        resync(input, byte);
    }
}

uint32_t board_input_keys(BoardInput *input, uint32_t now_ms)
{
    if (now_ms - input->last_valid_ms >= BOARD_UART_RELEASE_MS) input->keys = 0;
    if (input->used && now_ms - input->last_byte_ms > BOARD_UART_BYTE_TIMEOUT_MS)
        input->used = 0;
    return input->keys;
}

#ifndef BOARD_VIDEO_ADAPTER_EXTERNAL
int board_video_init(uint16_t *buffer0, uint16_t *buffer1,
                     unsigned width, unsigned height, size_t stride_pixels)
{
    (void)buffer0; (void)buffer1; (void)width; (void)height; (void)stride_pixels;
    return BOARD_UNAVAILABLE;
}
int board_video_acquire(unsigned *index) { (void)index; return BOARD_UNAVAILABLE; }
int board_video_present(unsigned index) { (void)index; return BOARD_UNAVAILABLE; }
#endif

#ifndef BOARD_G2D_ADAPTER_EXTERNAL
int board_g2d_init(void) { return BOARD_UNAVAILABLE; }
int board_g2d_acquire(uint32_t *frame_id)
{
    (void)frame_id;
    return BOARD_UNAVAILABLE;
}
int board_g2d_submit(void *context, const GameG2dCommand *command)
{
    (void)context;
    (void)command;
    return BOARD_UNAVAILABLE;
}
int board_g2d_wait(uint32_t frame_id)
{
    (void)frame_id;
    return BOARD_UNAVAILABLE;
}
int board_g2d_present(uint32_t frame_id)
{
    (void)frame_id;
    return BOARD_UNAVAILABLE;
}
#endif
