#include "render.h"
#include "assets.h"
#include <string.h>
#include "render_glyphs.h"
typedef struct { GameDrawList *list; gfx_image target; int playfield; } Scene;
gfx_error game_render_source(size_t index,gfx_image *out){
    const GameSprite *p=NULL;
    if(!out)return GFX_INVALID;
    if(index==0)p=&asset_background;
    else if(index<=3)p=&asset_player[index-1];
    else if(index==4)p=&asset_boss;
    else if(index==5)p=&asset_bullet_ring;
    else if(index==6)p=&asset_bullet_sweep;
    else if(index==7)p=&asset_shot;
    else if(index<GAME_RENDER_SOURCES){
        const unsigned char *atlas=index==8?glyph_white:index==9?glyph_yellow:glyph_grey;
        *out=(gfx_image){180,7,360,2520,GFX_RGB565_LE,(unsigned char *)(uintptr_t)atlas};
        return GFX_OK;
    }else return GFX_INVALID;
    *out=(gfx_image){p->width,p->height,(size_t)p->width*2,
          (size_t)p->width*p->height*2,GFX_RGB565_LE,(unsigned char *)(uintptr_t)p->pixels};
    return GFX_OK;
}
void game_draw_list_init(GameDrawList *list,size_t capacity) {
    if(!list)return;
    list->count=0;list->capacity=capacity;list->result=GFX_OK;
}
static int little_endian(void){const uint16_t x=1;return *(const unsigned char *)&x==1;}
static int overlaps(const void *a,size_t an,const void *b,size_t bn){
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return 1;
    return an&&bn&&x<y+bn&&y<x+an;
}
static void push(Scene *scene,gfx_command *command){
    GameDrawList *list=scene->list;gfx_command clipped;int empty;gfx_error e;
    if(list->result!=GFX_OK)return;
    e=gfx_clip_command(command,&clipped,&empty);
    if(e!=GFX_OK){list->result=e;return;}
    if(empty)return;
    if(list->count>=list->capacity){list->result=GFX_OVERFLOW;return;}
    list->commands[list->count++]=clipped;
}
/* Logical crop shifts both destination coordinates and source rectangle. */
static int area(Scene *s,int *x,int *y,int *w,int *h,int *sx,int *sy){
    int left=s->playfield?GAME_LEFT:0,top=s->playfield?GAME_TOP:0;
    int right=s->playfield?GAME_RIGHT+1:GAME_WIDTH;
    int bottom=s->playfield?GAME_BOTTOM+1:GAME_HEIGHT;
    int n;
    if(*x<left){n=left-*x;*x=left;*w-=n;if(sx)*sx+=n;}
    if(*y<top){n=top-*y;*y=top;*h-=n;if(sy)*sy+=n;}
    if(*x>=right||*y>=bottom||*w<=0||*h<=0)return 0;
    if(*w>right-*x)*w=right-*x;
    if(*h>bottom-*y)*h=bottom-*y;
    return *w>0&&*h>0;
}
static void fill(Scene *s,int x,int y,int w,int h,uint16_t color){
    gfx_command c={0};
    if(!area(s,&x,&y,&w,&h,NULL,NULL))return;
    c.operation=GFX_FILL;c.destination=s->target;
    c.rectangle=(gfx_rect){x,y,(uint32_t)w,(uint32_t)h};c.color=color;push(s,&c);
}
static void copy_region(Scene *s,gfx_image source,int sx,int sy,int x,int y,int w,int h){
    gfx_command c={0};
    if(!area(s,&x,&y,&w,&h,&sx,&sy))return;
    c.operation=GFX_COPY;c.destination=s->target;c.source=source;
    c.rectangle=(gfx_rect){x,y,(uint32_t)w,(uint32_t)h};
    c.source_x=sx;c.source_y=sy;c.flags=GFX_COLOR_KEY;c.key=GAME_COLOR_KEY;push(s,&c);
}
static void sprite(Scene *s,const GameSprite *sprite_value,int x,int y){
    gfx_image im={sprite_value->width,sprite_value->height,(size_t)sprite_value->width*2,
                  (size_t)sprite_value->width*sprite_value->height*2,GFX_RGB565_LE,
                  (unsigned char *)(uintptr_t)sprite_value->pixels};
    copy_region(s,im,0,0,x,y,sprite_value->width,sprite_value->height);
}
static void centered(Scene *s,const GameSprite *sprite_value,int32_t x,int32_t y){
    sprite(s,sprite_value,(int)(x/GAME_Q)-(int)sprite_value->width/2,
           (int)(y/GAME_Q)-(int)sprite_value->height/2);
}
static void text(Scene *s,int x,int y,const char *value,uint16_t color){
    const unsigned char *atlas=color==0xffe0?glyph_yellow:color==0x7bef?glyph_grey:glyph_white;
    gfx_image im={180,7,360,2520,GFX_RGB565_LE,(unsigned char *)(uintptr_t)atlas};
    for(;*value;++value,x+=6){int glyph=-1;
        if(*value>='0'&&*value<='9')glyph=*value-'0';
        else if(*value>='A'&&*value<='Z')glyph=*value-'A'+10;
        if(glyph>=0)copy_region(s,im,glyph*5,0,x,y,5,7);
    }
}
static void number(Scene *s,int x,int y,uint32_t value,uint16_t color){
    char buf[11],reverse[10];unsigned n=0,i;
    do{reverse[n++]=(char)('0'+value%10u);value/=10u;}while(value&&n<10);
    for(i=0;i<n;++i)buf[i]=reverse[n-1-i];
    buf[n]=0;text(s,x,y,buf,color);
}
gfx_error game_build_commands(const Game *g,const gfx_image *destination,GameDrawList *list){
    Scene s;gfx_command probe={0};unsigned i;gfx_error e;
    const GamePlayConfig *config;const uint16_t white=0xffff,yellow=0xffe0;
    if(!list)return GFX_INVALID;
    /* Alias rejection must itself not corrupt pixel/game/target storage.
     * Unlike ordinary failures, these errors preserve the list unchanged. */
    if((g&&overlaps(list,sizeof(*list),g,sizeof(*g)))||
       (destination&&(overlaps(list,sizeof(*list),destination,sizeof(*destination))||
        overlaps(list,sizeof(*list),destination->pixels,destination->bytes))))return GFX_INVALID;
    list->count=0;list->result=GFX_INVALID;
    if(!g||!destination)return list->result;
    if(!list->capacity||list->capacity>GAME_DRAW_CAPACITY)return list->result=GFX_OVERFLOW;
    if(!little_endian())return list->result=GFX_UNSUPPORTED;
    if(destination->format!=GFX_RGB565_LE)return list->result=GFX_UNSUPPORTED;
    if(destination->width!=GAME_WIDTH||destination->height!=GAME_HEIGHT)return list->result;
    probe.operation=GFX_FILL;probe.destination=*destination;
    probe.rectangle=(gfx_rect){0,0,GAME_WIDTH,GAME_HEIGHT};
    e=gfx_validate_command(&probe);if(e!=GFX_OK)return list->result=e;
    list->result=GFX_OK;s=(Scene){list,*destination,0};config=game_play_config(g);
    fill(&s,0,0,400,300,0x1004);sprite(&s,&asset_background,0,0);
    s.playfield=1;
    for(i=0;i<GAME_PLAYER_CAP;++i)if(g->player[i].active)centered(&s,&asset_shot,g->player[i].x,g->player[i].y);
    centered(&s,&asset_boss,g->boss_x,g->boss_y);
    for(i=0;i<GAME_ENEMY_CAP;++i)if(g->enemy[i].active)
        centered(&s,g->enemy[i].kind?&asset_bullet_sweep:&asset_bullet_ring,g->enemy[i].x,g->enemy[i].y);
    centered(&s,&asset_player[(g->prev_keys&GKEY_LEFT)?1:(g->prev_keys&GKEY_RIGHT)?2:0],g->player_x,g->player_y);
    if(g->invulnerable){
        int x=(int)(g->player_x/GAME_Q)-11,y=(int)(g->player_y/GAME_Q)-15;
        uint16_t color=((g->tick/6u)&1u)?0x07ff:white;
        fill(&s,x,y,22,1,color);fill(&s,x,y+29,22,1,color);
        fill(&s,x,y,1,30,color);fill(&s,x+21,y,1,30,color);
    }
    fill(&s,(int)(g->player_x/GAME_Q)-1,(int)(g->player_y/GAME_Q)-1,3,3,yellow);
    s.playfield=0;
    fill(&s,284,0,116,300,0x1084);
    fill(&s,10,10,272,1,white);fill(&s,10,288,272,1,white);
    fill(&s,10,10,1,279,white);fill(&s,281,10,1,279,white);
    text(&s,292,12,"FLANDRE",yellow);
    text(&s,292,28,g->mode==GMODE_DEMO?"DEMO":g->difficulty==GDIFFICULTY_BEGINNER?"BEGINNER":"STANDARD",white);
    text(&s,292,48,"HP",white);number(&s,292,59,g->hp,yellow);
    text(&s,292,77,"LIVES",white);number(&s,292,88,g->lives,yellow);
    text(&s,292,106,"SCORE",white);number(&s,292,117,g->score,yellow);
    text(&s,292,135,"BULLETS",white);number(&s,292,146,g->active_enemy,yellow);
    text(&s,292,164,"LIMIT",white);number(&s,292,175,g->enemy_limit,yellow);
    text(&s,292,193,"PEAK",white);number(&s,292,204,g->peak_enemy,yellow);
    text(&s,292,222,"DROPPED",white);number(&s,292,233,g->dropped,yellow);
    text(&s,292,251,config->auto_fire?"AUTO ON":"AUTO OFF",white);
    text(&s,292,262,g->invulnerable?"INVULN":"READY",yellow);
    text(&s,292,285,"RGB565",0x7bef);
    if(g->state!=GSTATE_RUNNING){
        const char *title=g->state==GSTATE_TITLE?"READY":g->state==GSTATE_PAUSED?"PAUSED":g->state==GSTATE_WON?"FINISHED":"GAME OVER";
        fill(&s,36,102,228,94,0);text(&s,48,110,title,yellow);
        text(&s,48,125,"ENTER START  D DEMO",white);
        text(&s,48,139,"N BEGINNER OR STANDARD",white);
        text(&s,48,153,"ARROWS MOVE  Z FIRE",white);
        text(&s,48,167,"TAB DENSITY  P PAUSE",white);
        text(&s,48,181,config->auto_fire?"AUTO FIRE ON":"HOLD Z TO FIRE",white);
    }
    if(list->result!=GFX_OK)list->count=0;
    return list->result;
}
