#include "board_port.h"
#include <assert.h>
#include <stdio.h>

static void send(BoardInput *p, uint16_t keys, uint32_t ms, int corrupt)
{
    uint8_t frame[8] = {0xa5, 0x5a, 1, 3, 9, (uint8_t)keys, (uint8_t)(keys >> 8), 0};
    unsigned i;
    frame[7] = board_uart_crc8(frame + 2, 5);
    if (corrupt) frame[7] ^= 1;
    for (i = 0; i < 8; ++i) board_input_feed(p, frame[i], ms);
}
int main(void)
{
    BoardInput p;
    uint16_t dummy[1];
    unsigned index = 77;
    board_input_init(&p, 0x1ffu);
    assert(board_input_keys(&p, 0) == 0);
    send(&p, 0x111u, 100, 0);
    assert(p.accepted == 1 && board_input_keys(&p, 349) == 0x111u);
    assert(board_input_keys(&p, 350) == 0);
    send(&p, 0x22u, 400, 0);
    send(&p, 0x44u, 401, 1);
    assert(p.accepted == 2 && board_input_keys(&p, 401) == 0x22u);
    send(&p, 0x8000u, 402, 0);
    assert(p.accepted == 2 && board_input_keys(&p, 402) == 0x22u);
    board_input_feed(&p, 0xa5, 500);
    board_input_feed(&p, 0x5a, 601); /* partial frame times out */
    assert(p.used == 0);
    board_input_feed(&p, 0xa5, 602);
    send(&p, 3u, 603, 0); /* A5 A5 5A resync */
    assert(p.accepted == 3 && board_input_keys(&p, 603) == 3);
    send(&p, 5u, UINT32_MAX - 100u, 0);
    assert(board_input_keys(&p, 100u) == 5u); /* timer wraps, 201ms */
    assert(board_input_keys(&p, 150u) == 0u);
    assert(board_video_init(dummy, dummy, 1, 1, 1) == BOARD_UNAVAILABLE);
    assert(board_video_acquire(&index) == BOARD_UNAVAILABLE && index == 77);
    assert(board_video_present(0) == BOARD_UNAVAILABLE);
    puts("board UART framing, CRC, resync, timeout and unavailable-video tests passed");
    return 0;
}
