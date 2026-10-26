#include "game.h"
#include "render.h"
#include "backend_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef GAME_HOST_SDL
#include <SDL2/SDL.h>
#endif
extern size_t gfx_test_alloc_calls;
static Game game;
static GameCpuRenderer cpu_renderer;
_Alignas(max_align_t) static unsigned char cpu_workspace[GFX_CPU_CONTEXT_BYTES_BOUND(GAME_DRAW_CAPACITY)];
static uint16_t pixels[400*300];
static int write_ppm(const char *path){
    FILE *f=fopen(path,"wb");unsigned i;
    if(!f)return 0;
    fprintf(f,"P6\n400 300\n255\n");
    for(i=0;i<400u*300u;++i){
        uint16_t p=pixels[i];unsigned r=(p>>11)&31,g=(p>>5)&63,b=p&31;
        unsigned char rgb[3]={(unsigned char)((r<<3)|(r>>2)),(unsigned char)((g<<2)|(g>>4)),(unsigned char)((b<<3)|(b>>2))};
        if(fwrite(rgb,1,3,f)!=3){fclose(f);return 0;}
    }
    return fclose(f)==0;
}
int main(int argc,char **argv){
    int headless=0,play=1;GameBackend backend=GAME_BACKEND_CPU;GameDifficulty difficulty=GDIFFICULTY_BEGINNER;gfx_error gfx_result;unsigned frames=1800,limit=256,i;
    const char *output="frame.ppm";
    for(i=1;i<(unsigned)argc;++i){
        if(!strcmp(argv[i],"--headless"))headless=1;
        else if(!strcmp(argv[i],"--play"))play=1;
        else if(!strcmp(argv[i],"--demo"))play=0;
        else if(!strcmp(argv[i],"--standard"))difficulty=GDIFFICULTY_STANDARD;
        else if(!strcmp(argv[i],"--beginner"))difficulty=GDIFFICULTY_BEGINNER;
        else if(!strcmp(argv[i],"--backend")&&i+1<(unsigned)argc){
            const char *v=argv[++i];
            if(!strcmp(v,"1"))backend=GAME_BACKEND_CPU;
            else if(!strcmp(v,"2")||!strcmp(v,"3")){
                fprintf(stderr,"Backend %s requires the matched Sapphire FPGA; this host executable supports CPU only.\n",v);return 6;
            }else{fprintf(stderr,"--backend must be 1, 2 or 3\n");return 2;}
        }
        else if(!strcmp(argv[i],"--frames")&&i+1<(unsigned)argc)frames=(unsigned)strtoul(argv[++i],NULL,10);
        else if(!strcmp(argv[i],"--limit")&&i+1<(unsigned)argc)limit=(unsigned)strtoul(argv[++i],NULL,10);
        else if(!strcmp(argv[i],"--ppm")&&i+1<(unsigned)argc)output=argv[++i];
        else {fprintf(stderr,"usage: %s [--headless] [--play|--demo] [--beginner|--standard] [--backend 1] [--frames N] [--limit 128|256|512] [--ppm file]\n",argv[0]);return 2;}
    }
#ifndef GAME_HOST_SDL
    headless=1;
    (void)backend;
#endif
    gfx_result=game_cpu_renderer_init(&cpu_renderer,cpu_workspace,sizeof(cpu_workspace));
    if(gfx_result!=GFX_OK){fprintf(stderr,"graphics init: %s\n",gfx_error_string(gfx_result));return 5;}
    game_init_config(&game,play?GMODE_PLAY:GMODE_DEMO,limit,difficulty);
    if(headless){
        for(i=0;i<frames;++i)game_step(&game,(play&&i==0)?GKEY_START:0);
        gfx_result=game_render_cpu(&cpu_renderer,&game,pixels,400,400u*300u,0);
        if(gfx_result!=GFX_OK){fprintf(stderr,"graphics frame: %s\n",gfx_error_string(gfx_result));return 5;}
        printf("frames=%u tick=%u state=%u enemy=%u peak=%u dropped=%u hash=%08x game_bytes=%zu framebuffer_bytes=%zu\n",
               frames,(unsigned)game.tick,(unsigned)game.state,(unsigned)game.active_enemy,(unsigned)game.peak_enemy,
               (unsigned)game.dropped,(unsigned)game_hash(&game),sizeof(game),sizeof(pixels));
        printf("commands=%zu peak_commands=%zu command_bytes=%zu context_used=%zu context_reserved=%zu allocation_calls=%zu\n",cpu_renderer.frame.count,cpu_renderer.peak_commands,sizeof(cpu_renderer.frame.commands),cpu_renderer.workspace_required,cpu_renderer.workspace_reserved,gfx_test_alloc_calls);
        if(!write_ppm(output)){perror(output);return 3;}
        return 0;
    }
#ifdef GAME_HOST_SDL
    {
        SDL_Window *win;SDL_Renderer *renderer;SDL_Texture *texture;
        uint64_t last,freq,accum=0;unsigned steps=0;int running=1;
        uint32_t pending=0;
        const char *window_title="Ti60 Flandre | ENTER start | N beginner/standard | arrows + Z | 1 CPU | 2/3 on FPGA board";
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_TIMER)!=0){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 4;}
        win=SDL_CreateWindow(window_title,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,800,600,SDL_WINDOW_RESIZABLE);
        renderer=win?SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE):NULL;
        texture=renderer?SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB565,SDL_TEXTUREACCESS_STREAMING,400,300):NULL;
        if(!texture){fprintf(stderr,"SDL: %s\n",SDL_GetError());if(renderer)SDL_DestroyRenderer(renderer);if(win)SDL_DestroyWindow(win);SDL_Quit();return 4;}
        SDL_RenderSetLogicalSize(renderer,400,300);
        last=SDL_GetPerformanceCounter();freq=SDL_GetPerformanceFrequency();
        while(running){
            SDL_Event e;const Uint8 *held;uint32_t keys=0,selection=0;uint64_t now,delta;
            while(SDL_PollEvent(&e)){
                if(e.type==SDL_QUIT)running=0;
                if(e.type==SDL_KEYDOWN&&!e.key.repeat){
                    switch(e.key.keysym.sym){
                    case SDLK_RETURN: case SDLK_r: pending|=GKEY_START;break;
                    case SDLK_p: pending|=GKEY_PAUSE;break;
                    case SDLK_d: pending|=GKEY_DEMO;break;
                    case SDLK_TAB: pending|=GKEY_LEVEL;break;
                    case SDLK_n: pending|=GKEY_DIFFICULTY;break;
                    case SDLK_1: case SDLK_KP_1: selection|=GKEY_BACKEND_CPU;break;
                    case SDLK_2: case SDLK_KP_2: selection|=GKEY_BACKEND_G2D;break;
                    case SDLK_3: case SDLK_KP_3: selection|=GKEY_BACKEND_COMPARE;break;
                    default: break;
                    }
                }
            }
            if(selection){
                GameBackend request=game_backend_select(backend,selection,0);
                if(request!=GAME_BACKEND_CPU){
                    fprintf(stderr,"Request %u rejected: G2D/compare require the Sapphire FPGA, CPU preview remains selected.\n",(unsigned)request);
                    SDL_SetWindowTitle(win,"G2D/compare unavailable on this PC; connect the UART bridge to the FPGA. Press 1 for CPU.");
                }else{backend=request;SDL_SetWindowTitle(win,window_title);}
            }
            held=SDL_GetKeyboardState(NULL);
            if(held[SDL_SCANCODE_ESCAPE])running=0;
            if(held[SDL_SCANCODE_LEFT])keys|=GKEY_LEFT;
            if(held[SDL_SCANCODE_RIGHT])keys|=GKEY_RIGHT;
            if(held[SDL_SCANCODE_UP])keys|=GKEY_UP;
            if(held[SDL_SCANCODE_DOWN])keys|=GKEY_DOWN;
            if(held[SDL_SCANCODE_Z]||held[SDL_SCANCODE_SPACE])keys|=GKEY_FIRE;
            if(held[SDL_SCANCODE_RETURN]||held[SDL_SCANCODE_R])keys|=GKEY_START;
            if(held[SDL_SCANCODE_P])keys|=GKEY_PAUSE;
            if(held[SDL_SCANCODE_D])keys|=GKEY_DEMO;
            if(held[SDL_SCANCODE_TAB])keys|=GKEY_LEVEL;
            if(held[SDL_SCANCODE_N])keys|=GKEY_DIFFICULTY;
            if(!(SDL_GetWindowFlags(win)&SDL_WINDOW_INPUT_FOCUS)){keys=0;pending=0;}
            now=SDL_GetPerformanceCounter();delta=now-last;last=now;
            if(delta>freq/4)delta=freq/4;
            accum+=delta*60u;
            while(accum>=freq){game_step(&game,keys|pending);pending=0;++steps;accum-=freq;}
            if(steps>=2){
                gfx_result=game_render_cpu(&cpu_renderer,&game,pixels,400,400u*300u,0);
                if(gfx_result!=GFX_OK){fprintf(stderr,"graphics frame: %s\n",gfx_error_string(gfx_result));break;}
                if(SDL_UpdateTexture(texture,NULL,pixels,800)||SDL_RenderClear(renderer)||SDL_RenderCopy(renderer,texture,NULL,NULL)){
                    fprintf(stderr,"SDL rendering: %s\n",SDL_GetError());running=0;
                }
                SDL_RenderPresent(renderer);steps=0;
            }
            SDL_Delay(1);
        }
        SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(win);SDL_Quit();
    }
#endif
    return gfx_result==GFX_OK?0:5;
}
