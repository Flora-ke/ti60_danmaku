#include "hardware_render.h"
#include "hardware_memory.h"

static gfx_error fault(GameHardwareRenderer *r,gfx_error error){
    r->phase=GAME_HW_FAULT;r->error=error;return error;
}
gfx_error game_hardware_renderer_init(GameHardwareRenderer *r,const GameHardwareAssets *assets,
    void *cpu_storage,size_t cpu_bytes,void *g2d_storage,size_t g2d_bytes,const gfx_g2d_config *config){
    gfx_g2d_config bound;gfx_error e;size_t alignment;
    if(!r||!assets||!assets->initialized||!config||r->assets||r->job)return GFX_INVALID;
    const void *regions[]={r,assets,cpu_storage,g2d_storage,config,config->device};
    const size_t sizes[]={sizeof *r,sizeof *assets,cpu_bytes,g2d_bytes,sizeof *config,
                         config->device?sizeof *config->device:0};
    /* Context headers and command snapshots must never overwrite each other,
     * registry metadata, source pixels or an IO device object. */
    for(size_t a=0;a<4;a++){
        for(size_t b=a+1;b<6;b++)
            if(game_storage_overlap(regions[a],sizes[a],regions[b],sizes[b]))return GFX_INVALID;
        for(size_t i=0;i<GAME_DMA_ASSET_COUNT;i++)
            if(game_storage_overlap(regions[a],sizes[a],assets->assets[i].allocation.cpu_begin,
                                    assets->assets[i].allocation.bytes))return GFX_INVALID;
    }
    bound=*config;
    r->assets=assets;
    e=game_cpu_renderer_init(&r->cpu,cpu_storage,cpu_bytes);if(e!=GFX_OK)return fault(r,e);
    bound.assets=assets->assets;bound.asset_count=GAME_DMA_ASSET_COUNT;
    e=gfx_g2d_context_requirements(GAME_DRAW_CAPACITY,&r->g2d_workspace_bytes,&alignment);
    if(e!=GFX_OK)return fault(r,e);
    e=gfx_g2d_context_init(g2d_storage,g2d_bytes,GAME_DRAW_CAPACITY,&bound,&r->g2d);
    if(e!=GFX_OK)return fault(r,e);
    r->phase=GAME_HW_IDLE;r->error=GFX_OK;return GFX_OK;
}
static gfx_error native(GameHardwareRenderer *r){
    gfx_error e=game_hardware_target(&r->cpu.frame,r->assets,GFX_DESTINATION_FRAME);
    if(e!=GFX_OK)return fault(r,e);
    r->phase=GAME_HW_NATIVE;
    e=gfx_g2d_submit(r->g2d,r->cpu.frame.commands,r->cpu.frame.count,&r->job);
    return e==GFX_OK?GFX_BUSY:fault(r,e);
}
static gfx_error publish_cpu(GameHardwareRenderer *r){
    gfx_command commands[2]={{0}};gfx_error e;
    commands[0].operation=GFX_FILL;commands[0].target=GFX_DESTINATION_FRAME;
    commands[0].destination=gfx_g2d_frame_canvas();commands[0].rectangle=(gfx_rect){0,0,640,480};
    commands[0].color=0x1004;
    commands[1]=commands[0];commands[1].operation=GFX_COPY;
    commands[1].source=r->assets->canvas;
    r->phase=GAME_HW_CPU_PRESENT;
    e=gfx_g2d_submit(r->g2d,commands,2,&r->job);
    return e==GFX_OK?GFX_BUSY:fault(r,e);
}
gfx_error game_hardware_start(GameHardwareRenderer *r,const Game *g,GameBackend backend){
    gfx_error e;uint64_t begin,end;
    if(!r||!r->assets)return GFX_INVALID;
    if(r->phase==GAME_HW_FAULT)return r->error;
    if(r->job||r->phase!=GAME_HW_IDLE)return GFX_BUSY;
    if(backend<GAME_BACKEND_CPU||backend>GAME_BACKEND_COMPARE)return GFX_INVALID;
    e=game_hardware_commands(g,r->assets,&r->cpu.frame);if(e!=GFX_OK)return fault(r,e);
    r->active=backend;r->frames_started++;
    if(r->cpu.frame.count>r->peak_commands)r->peak_commands=r->cpu.frame.count;
    if(backend==GAME_BACKEND_G2D)return native(r);
    e=gfx_clock_now(&begin);if(e!=GFX_OK)return fault(r,e);
    e=game_cpu_execute(&r->cpu,&r->cpu.frame,0);if(e!=GFX_OK)return fault(r,e);
    e=gfx_clock_now(&end);if(e!=GFX_OK||end<begin)return fault(r,e==GFX_OK?GFX_PLATFORM_ERROR:e);
    r->cpu_draw_ticks=end-begin;
    return publish_cpu(r);
}
gfx_error game_hardware_poll(GameHardwareRenderer *r){
    gfx_status status;gfx_error e;gfx_g2d_progress progress;
    if(!r||!r->assets)return GFX_INVALID;
    if(r->phase==GAME_HW_FAULT)return r->error;
    if(!r->job)return r->phase==GAME_HW_IDLE?GFX_OK:GFX_INVALID;
    e=gfx_query(r->job,&status);if(e!=GFX_OK)return fault(r,e);
    if(status.state==GFX_PENDING)return GFX_BUSY;
    if(status.state!=GFX_COMPLETE||status.result!=GFX_OK)
        return fault(r,status.result==GFX_OK?GFX_EXECUTION_FAILED:status.result);
    e=gfx_g2d_job_progress(r->job,&progress);if(e!=GFX_OK)return fault(r,e);
    if(!progress.frame.presented||!progress.frame.healthy)return fault(r,GFX_PLATFORM_ERROR);
    if(r->phase==GAME_HW_NATIVE)r->last_native=progress;else r->last_display=progress;
    e=gfx_job_release(&r->job);if(e!=GFX_OK)return fault(r,e);
    r->presented++;
    if(r->phase==GAME_HW_CPU_PRESENT&&r->active==GAME_BACKEND_COMPARE)return native(r);
    if(r->active==GAME_BACKEND_COMPARE)r->compare_pairs++;
    r->frames_completed++;r->phase=GAME_HW_IDLE;return GFX_OK;
}
