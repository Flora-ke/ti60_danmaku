#include "render.h"
#include <string.h>
gfx_error game_cpu_renderer_init(GameCpuRenderer *r,void *storage,size_t bytes){
    gfx_error e;size_t alignment;
    if(!r)return GFX_INVALID;
    memset(r,0,sizeof(*r));game_draw_list_init(&r->frame,GAME_DRAW_CAPACITY);
    e=gfx_cpu_context_requirements(GAME_DRAW_CAPACITY,&r->workspace_required,&alignment);
    if(e!=GFX_OK)return e;
    r->workspace_reserved=bytes;
    return gfx_cpu_context_init(storage,bytes,GAME_DRAW_CAPACITY,&r->cpu);
}
gfx_error game_cpu_finish(GameCpuRenderer *r,uint64_t timeout){
    gfx_error e,released;gfx_status status;
    if(!r||!r->job)return GFX_INVALID;
    e=gfx_wait(r->job,timeout);
    if(gfx_query(r->job,&status)!=GFX_OK)return GFX_PLATFORM_ERROR;
    if(status.state==GFX_PENDING)return e==GFX_OK?GFX_BUSY:e;
    if(e==GFX_OK)e=status.result;
    if(e==GFX_OK&&status.state!=GFX_COMPLETE)e=GFX_EXECUTION_FAILED;
    released=gfx_job_release(&r->job);
    if(e!=GFX_OK)return e;
    if(released!=GFX_OK)return released;
    ++r->completed_frames;return GFX_OK;
}
gfx_error game_cpu_execute(GameCpuRenderer *r,const GameDrawList *list,uint64_t timeout){
    gfx_error e;
    if(!r||!r->cpu||!list)return GFX_INVALID;
    if(r->job)return GFX_BUSY;
    if(list->result!=GFX_OK)return list->result;
    if(!list->count||list->count>list->capacity||list->capacity>GAME_DRAW_CAPACITY)return GFX_INVALID;
    e=gfx_cpu_submit_context(r->cpu,list->commands,list->count,&r->job);
    if(e!=GFX_OK)return e;
    ++r->submitted_frames;
    if(list->count>r->peak_commands)r->peak_commands=list->count;
    return game_cpu_finish(r,timeout);
}
gfx_error game_render_cpu(GameCpuRenderer *r,const Game *g,uint16_t *pixels,
                          size_t stride,size_t storage_pixels,uint64_t timeout){
    gfx_image target;size_t byte_stride,bytes;gfx_error e;
    if(!r||!r->cpu)return GFX_INVALID;
    if(r->job)return GFX_BUSY;
    e=gfx_size_mul(stride,2,&byte_stride);if(e!=GFX_OK)return e;
    e=gfx_size_mul(storage_pixels,2,&bytes);if(e!=GFX_OK)return e;
    target=(gfx_image){GAME_WIDTH,GAME_HEIGHT,byte_stride,bytes,GFX_RGB565_LE,(unsigned char *)pixels};
    e=game_build_commands(g,&target,&r->frame);if(e!=GFX_OK)return e;
    return game_cpu_execute(r,&r->frame,timeout);
}
