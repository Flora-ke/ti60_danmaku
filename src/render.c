#include "render.h"
#include "assets.h"

typedef struct { uint16_t *p; size_t stride; int playfield_clip; } Canvas;
static void rect(Canvas c,int x,int y,int w,int h,uint16_t color) {
    int xx,yy;
    if(x<0){w+=x;x=0;} if(y<0){h+=y;y=0;}
    if(x+w>400)w=400-x;
    if(y+h>300)h=300-y;
    if(w<=0||h<=0)return;
    for(yy=y;yy<y+h;++yy)for(xx=x;xx<x+w;++xx)c.p[(size_t)yy*c.stride+(size_t)xx]=color;
}
static void sprite(Canvas c,const GameSprite *s,int x,int y) {
    int sx,sy;
    for(sy=0;sy<(int)s->height;++sy){
        int yy=y+sy;if(yy<0||yy>=300)continue;
        if(c.playfield_clip&&(yy<GAME_TOP||yy>GAME_BOTTOM))continue;
        for(sx=0;sx<(int)s->width;++sx){
            int xx=x+sx;uint16_t color;
            if(xx<0||xx>=400)continue;
            if(c.playfield_clip&&(xx<GAME_LEFT||xx>GAME_RIGHT))continue;
            color=s->pixels[(size_t)sy*s->width+(size_t)sx];
            if(color!=GAME_COLOR_KEY)c.p[(size_t)yy*c.stride+(size_t)xx]=color;
        }
    }
}
static void centered(Canvas c,const GameSprite *s,int32_t x,int32_t y) {
    sprite(c,s,(int)(x/256)-(int)s->width/2,(int)(y/256)-(int)s->height/2);
}
/* Original 5x7 bitmap alphabet; no runtime font library or font file. */
static const uint8_t glyphs[36][7]={
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14},{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
 {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},
 {31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
 {14,4,4,4,4,4,14},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},
 {16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
 {14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},
 {30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
 {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},
 {17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};
static void text(Canvas c,int x,int y,const char *s,uint16_t color) {
    for(;*s;++s,x+=6){int g=-1,r,b;
        if(*s>='0'&&*s<='9')g=*s-'0';
        if(*s>='A'&&*s<='Z')g=*s-'A'+10;
        if(g<0)continue;
        for(r=0;r<7;++r)for(b=0;b<5;++b)
            if(glyphs[g][r]&(1u<<(4-b)))rect(c,x+b,y+r,1,1,color);
    }
}
static void number(Canvas c,int x,int y,uint32_t v,uint16_t color) {
    char buf[11];unsigned n=0,i;char tmp[10];
    do{tmp[n++]=(char)('0'+v%10u);v/=10u;}while(v&&n<10);
    for(i=0;i<n;++i)buf[i]=tmp[n-1-i];
    buf[n]=0;text(c,x,y,buf,color);
}
void game_render(const Game *g,uint16_t *pixels,size_t stride_pixels){
    Canvas c={pixels,stride_pixels,0};unsigned i;const uint16_t white=0xffff,yellow=0xffe0;
    if(!g||!pixels||stride_pixels<400)return;
    rect(c,0,0,400,300,0x1004);sprite(c,&asset_background,0,0);
    c.playfield_clip=1;
    for(i=0;i<64;++i)if(g->player[i].active)centered(c,&asset_shot,g->player[i].x,g->player[i].y);
    centered(c,&asset_boss,g->boss_x,g->boss_y);
    for(i=0;i<512;++i)if(g->enemy[i].active)
        centered(c,g->enemy[i].kind?&asset_bullet_sweep:&asset_bullet_ring,g->enemy[i].x,g->enemy[i].y);
    if(!g->invulnerable||((g->tick/6u)&1u)==0)
        centered(c,&asset_player[(g->prev_keys&GKEY_LEFT)?1:(g->prev_keys&GKEY_RIGHT)?2:0],g->player_x,g->player_y);
    c.playfield_clip=0;
    rect(c,284,0,116,300,0x1084);
    rect(c,10,10,272,1,white);rect(c,10,288,272,1,white);
    rect(c,10,10,1,279,white);rect(c,281,10,1,279,white);
    text(c,292,12,"FLANDRE",yellow);
    text(c,292,28,g->mode==GMODE_DEMO?"DEMO":"PLAY",white);
    text(c,292,48,"HP",white);number(c,292,59,g->hp,yellow);
    text(c,292,77,"LIVES",white);number(c,292,88,g->lives,yellow);
    text(c,292,106,"SCORE",white);number(c,292,117,g->score,yellow);
    text(c,292,135,"BULLETS",white);number(c,292,146,g->active_enemy,yellow);
    text(c,292,164,"LIMIT",white);number(c,292,175,g->enemy_limit,yellow);
    text(c,292,193,"PEAK",white);number(c,292,204,g->peak_enemy,yellow);
    text(c,292,222,"DROPPED",white);number(c,292,233,g->dropped,yellow);
    text(c,292,251,"SECONDS",white);number(c,292,262,g->tick/60u,yellow);
    text(c,292,285,"CPU RGB565",0x7bef);
    if(g->state!=GSTATE_RUNNING){
        const char *title=g->state==GSTATE_TITLE?"READY":g->state==GSTATE_PAUSED?"PAUSED":g->state==GSTATE_WON?"FINISHED":"GAME OVER";
        rect(c,58,116,178,66,0x0000);text(c,76,125,title,yellow);
        text(c,70,143,"ENTER START  D DEMO",white);
        text(c,70,160,"TAB LIMIT  P PAUSE",white);
    }
}
