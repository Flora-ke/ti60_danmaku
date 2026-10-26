/* Matched ordinary-CPU Sapphire entry: a single G2D owner for all modes.
 * The old sapphire_main.c remains available for explicit RAM diagnostics. */
#include "hardware_render.h"
#include "board_port.h"
#include "gfx_sapphire_publish.h"
#include "gfx_sapphire_display.h"
#include "gfx_sapphire_g2d.h"
#include "bsp.h"

#if !defined(__riscv_xlen) || __riscv_xlen != 32
#error This entry requires the matched RV32 Sapphire BSP
#endif
_Static_assert(SYSTEM_DDR_BMB==0x1000u&&SYSTEM_DDR_BMB_SIZE==0x10000000u,"Reviewed CPU DDR mapping");
static Game game;
static BoardInput input;
static GameHardwareAssets resources;
static GameHardwareRenderer renderer;
static gfx_g2d_v3 device;
_Alignas(64) static unsigned char game_dma_storage[GAME_DMA_STORAGE_BYTES];
_Alignas(64) static unsigned char game_cpu_frame[GAME_CANVAS_BYTES];
_Alignas(max_align_t) static unsigned char cpu_workspace[GFX_CPU_CONTEXT_BYTES_BOUND(GAME_DRAW_CAPACITY)];
_Alignas(max_align_t) static unsigned char g2d_workspace[GFX_G2D_CONTEXT_BYTES_BOUND(GAME_DRAW_CAPACITY)];
volatile int board_game_error;
volatile unsigned board_uart_timeout;

static uint64_t now_ticks(void){return clint_getTime(BSP_CLINT);}
static int put_char(char c){
    for(unsigned i=0;i<100000u;i++)if(uart_writeAvailability(BSP_UART_TERMINAL)){
        write_u32((uint32_t)(uint8_t)c,BSP_UART_TERMINAL+UART_DATA);return 1;
    }
    board_uart_timeout=1;return 0;
}
static void text(const char *s){if(!board_uart_timeout)while(*s)if(!put_char(*s++))return;}
static void hex32(uint32_t x){
    static const char digits[]="0123456789abcdef";
    for(int s=28;s>=0;s-=4)if(!put_char(digits[(x>>s)&15u]))return;
}
static void hex64(uint64_t x){hex32((uint32_t)(x>>32));hex32((uint32_t)x);}
static void stop(gfx_error error,const char *where){
    board_game_error=error;text("STOP ");text(where);text(" error=");hex32((uint32_t)error);text("\r\n");
    /* No cancel/reset/END on failure: preserve the job, source and frame lease. */
    for(;;){}
}
static uint32_t keys_now(uint64_t now){
    uint32_t ms=(uint32_t)(now/(BSP_CLINT_HZ/1000u));
    for(unsigned n=0;n<64u&&uart_readOccupancy(BSP_UART_TERMINAL);n++)
        board_input_feed(&input,(uint8_t)read_u32(BSP_UART_TERMINAL+UART_DATA),ms);
    return board_input_keys(&input,ms);
}
static gfx_g2d_memory owned_memory(unsigned char *p,size_t bytes){
    /* Only the two named ELF-owned BSS objects above are passed here. The final
     * ELF/map checker verifies their section/range/size, not arbitrary pointers. */
    uintptr_t a=(uintptr_t)p;
    if(a<0x1000u||bytes>0x10000000u-a||a>=0x10000000u)return (gfx_g2d_memory){0};
    return (gfx_g2d_memory){p,(uint32_t)a,bytes};
}
int main(void){
    gfx_cache_callbacks cache;gfx_error e;gfx_g2d_config config;
    uint64_t previous,now,elapsed,last_log,logic=0,draw=0;
    uint32_t previous_keys=0,late=0;GameBackend requested=GAME_BACKEND_CPU;
    bsp_init();board_input_init(&input,GAME_INPUT_MASK);
    text("Ti60 Flandre: 1 CPU+display-copy, 2 G2D, 3 same-scene comparison.\r\n");
    text("Enter start; arrows/WASD move; N beginner/standard; P pause; M demo; K density.\r\n");
    e=gfx_sapphire_publish_cache(&cache);if(e!=GFX_OK)stop(e,"DDR publication provider");
    e=game_hardware_assets_init(&resources,owned_memory(game_dma_storage,sizeof game_dma_storage),
                               owned_memory(game_cpu_frame,sizeof game_cpu_frame));
    if(e!=GFX_OK)stop(e,"owned DMA sources");
    e=gfx_g2d_v3_probe(&device,gfx_sapphire_g2d_io());if(e!=GFX_OK)stop(e,"G2D ABI/CAPS probe");
    config=(gfx_g2d_config){.device=&device,.display=gfx_sapphire_display_io(),.cache=cache,
        .cache_line_bytes=SYSTEM_CORES_0_BYTES_PER_LINE,.capture_polls=128,
        .frame_timeout_ticks=(uint64_t)BSP_CLINT_HZ*5u,.first_frame=1,.first_sequence=1,.measure_enabled=1};
    e=game_hardware_renderer_init(&renderer,&resources,cpu_workspace,sizeof cpu_workspace,
                                  g2d_workspace,sizeof g2d_workspace,&config);
    if(e!=GFX_OK)stop(e,"cold G2D/display session");
    game_init(&game,GMODE_PLAY,256);
    previous=last_log=now_ticks();
    for(;;){
        uint32_t keys;unsigned steps;
        now=now_ticks();elapsed=now-previous;previous=now;keys=keys_now(now);
        requested=game_backend_select(requested,keys,previous_keys);previous_keys=keys;
        if(elapsed>BSP_CLINT_HZ/4u){elapsed=BSP_CLINT_HZ/4u;late++;}
        logic+=elapsed*GAME_HZ;draw+=elapsed*30u;
        for(steps=0;logic>=BSP_CLINT_HZ&&steps<8u;steps++){
            /* Render selection is platform input, and never changes Game/hash. */
            game_step(&game,keys&0x03ffu);logic-=BSP_CLINT_HZ;
        }
        if(logic>=BSP_CLINT_HZ){logic%=BSP_CLINT_HZ;late++;}
        e=game_hardware_poll(&renderer);
        if(e!=GFX_OK&&e!=GFX_BUSY)stop(e,"frame progress (resources retained)");
        if(draw>=BSP_CLINT_HZ&&renderer.phase==GAME_HW_IDLE){
            draw%=BSP_CLINT_HZ;e=game_hardware_start(&renderer,&game,requested);
            if(e!=GFX_OK&&e!=GFX_BUSY)stop(e,"frame build/submit");
        }
        if(now-last_log>=BSP_CLINT_HZ){
            last_log=now;text("mode=");hex32((uint32_t)renderer.active);
            text(" req=");hex32((uint32_t)requested);text(" tick=");hex32(game.tick);
            text(" shown=");hex32(renderer.presented);text(" pair=");hex32(renderer.compare_pairs);
            text(" cmds=");hex32((uint32_t)renderer.cpu.frame.count);text(" late=");hex32(late);
            text(" cpu_draw=");hex64(renderer.cpu_draw_ticks);
            text(" cpu_seq=");hex32(renderer.cpu.completed_frames);
            text(" gpu_frame=");hex32(renderer.last_native.frame.identity.frame_id);
            text(" gpu_valid=");hex32((uint32_t)(renderer.last_native.metrics.enabled&&renderer.last_native.metrics.valid));
            text(" gpu_error=");hex32((uint32_t)renderer.last_native.metrics.error);
            if(renderer.last_native.metrics.enabled&&renderer.last_native.metrics.valid){
                text(" gpu_total=");hex64(renderer.last_native.metrics.total_ticks);
                text(" gpu_present=");hex64(renderer.last_native.metrics.present_wait_ticks);
            }else text(" gpu_total=NA gpu_present=NA");
            text(" hz=");hex32(BSP_CLINT_HZ);text("\r\n");
        }
    }
}
