#include "hardware_render.h"
#include <string.h>
#include "hardware_memory.h"

static size_t round_up(size_t n,size_t alignment){return (n+alignment-1u)&~(alignment-1u);}
static int same_image(const gfx_image *a,const gfx_image *b){
    return a->width==b->width&&a->height==b->height&&a->stride==b->stride&&
        a->bytes==b->bytes&&a->pixels==b->pixels&&a->format==b->format;
}
gfx_error game_hardware_assets_init(GameHardwareAssets *r,gfx_g2d_memory storage,
                                   gfx_g2d_memory framebuffer){
    size_t used=0;gfx_error e;const uint16_t endian=1;
    if(!r||r->initialized)return GFX_INVALID;
    if(*(const unsigned char *)&endian!=1)return GFX_UNSUPPORTED;
    if(!storage.cpu_begin||!framebuffer.cpu_begin||
       ((uintptr_t)storage.cpu_begin&63u)||((uintptr_t)framebuffer.cpu_begin&63u)||
       (storage.device_begin&63u)||(framebuffer.device_begin&63u)||
       storage.bytes<GAME_DMA_STORAGE_BYTES||framebuffer.bytes<GAME_CANVAS_BYTES||
       game_storage_overlap(storage.cpu_begin,storage.bytes,framebuffer.cpu_begin,framebuffer.bytes)||
       game_storage_overlap(r,sizeof *r,storage.cpu_begin,storage.bytes)||
       game_storage_overlap(r,sizeof *r,framebuffer.cpu_begin,framebuffer.bytes)||
       ((uint64_t)storage.device_begin<(uint64_t)framebuffer.device_begin+framebuffer.bytes&&
        (uint64_t)framebuffer.device_begin<(uint64_t)storage.device_begin+storage.bytes))return GFX_INVALID;
    /* Preflight every allocation before changing caller-owned source pixels. */
    for(size_t i=0;i<GAME_RENDER_SOURCES;i++){
        gfx_image source;size_t stride,bytes;gfx_g2d_sprite_plan plan;
        e=game_render_source(i,&source);if(e!=GFX_OK)return e;
        if(game_storage_overlap(source.pixels,source.bytes,r,sizeof *r)||
           game_storage_overlap(source.pixels,source.bytes,storage.cpu_begin,storage.bytes)||
           game_storage_overlap(source.pixels,source.bytes,framebuffer.cpu_begin,framebuffer.bytes))return GFX_INVALID;
        stride=round_up(source.width*2u,16);bytes=round_up(stride*source.height,64);
        if(bytes>storage.bytes-used||(uint64_t)storage.device_begin+used+bytes>UINT32_MAX)return GFX_OVERFLOW;
        r->originals[i]=source;
        r->assets[i]=(gfx_g2d_asset){
            {storage.cpu_begin+used,storage.device_begin+(uint32_t)used,bytes},
            {source.width,source.height,stride,bytes,GFX_RGB565_LE,storage.cpu_begin+used}};
        gfx_g2d_sprite sprite={.asset=r->assets[i],.rectangle={0,0,source.width,source.height}};
        e=gfx_g2d_plan_sprite(&sprite,&plan);if(e!=GFX_OK)return e;
        used+=bytes;
    }
    r->canvas=(gfx_image){GAME_CANVAS_WIDTH,GAME_CANVAS_HEIGHT,GAME_CANVAS_WIDTH*2u,
                         GAME_CANVAS_BYTES,GFX_RGB565_LE,framebuffer.cpu_begin};
    r->assets[GAME_RENDER_SOURCES]=(gfx_g2d_asset){framebuffer,r->canvas};
    gfx_g2d_sprite final={.asset=r->assets[GAME_RENDER_SOURCES],.rectangle={0,0,640,480}};
    gfx_g2d_sprite_plan p;e=gfx_g2d_plan_sprite(&final,&p);if(e!=GFX_OK)return e;
    for(size_t i=0;i<GAME_RENDER_SOURCES;i++){
        const gfx_image *source=&r->originals[i];gfx_g2d_asset *asset=&r->assets[i];
        memset(asset->allocation.cpu_begin,0,asset->allocation.bytes);
        for(size_t y=0;y<source->height;y++)
            memcpy(asset->image.pixels+y*asset->image.stride,source->pixels+y*source->stride,source->width*2u);
    }
    r->packed_bytes=used;r->initialized=1;return GFX_OK;
}
gfx_error game_hardware_commands(const Game *g,const GameHardwareAssets *r,GameDrawList *list){
    gfx_image logical;gfx_error e;
    if(!r||!r->initialized||!list)return GFX_INVALID;
    logical=r->canvas;logical.width=GAME_WIDTH;logical.height=GAME_HEIGHT;
    e=game_build_commands(g,&logical,list);if(e!=GFX_OK)return e;
    for(size_t n=0;n<list->count;n++){
        gfx_command *c=&list->commands[n];
        if(c->operation==GFX_COPY){
            size_t i;for(i=0;i<GAME_RENDER_SOURCES;i++)if(same_image(&c->source,&r->originals[i]))break;
            if(i==GAME_RENDER_SOURCES){list->count=0;return list->result=GFX_UNSUPPORTED;}
            c->source=r->assets[i].image;
        }
        c->destination=r->canvas;
        if(!n)c->rectangle=(gfx_rect){0,0,640,480};
        else{c->rectangle.x+=120;c->rectangle.y+=90;}
        e=gfx_validate_command(c);if(e!=GFX_OK){list->count=0;return list->result=e;}
    }
    return GFX_OK;
}
gfx_error game_hardware_target(GameDrawList *list,const GameHardwareAssets *r,gfx_destination_kind target){
    gfx_image image;gfx_command probe;gfx_error e;
    if(!list||!r||!r->initialized||list->result!=GFX_OK||!list->count||
       list->count>list->capacity||list->capacity>GAME_DRAW_CAPACITY)return GFX_INVALID;
    if(target!=GFX_DESTINATION_CPU&&target!=GFX_DESTINATION_FRAME)return GFX_UNSUPPORTED;
    image=target==GFX_DESTINATION_CPU?r->canvas:gfx_g2d_frame_canvas();
    for(size_t i=0;i<list->count;i++){
        probe=list->commands[i];probe.destination=image;probe.target=target;
        e=gfx_validate_command(&probe);if(e!=GFX_OK)return e;
    }
    for(size_t i=0;i<list->count;i++){list->commands[i].destination=image;list->commands[i].target=target;}
    return GFX_OK;
}
