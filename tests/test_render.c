#include "render.h"
#include <assert.h>
#include <stdio.h>
static Game game;
static uint16_t guarded[302][404];
int main(void){
    unsigned x,y,i;game_init(&game,GMODE_DEMO,512);
    for(y=0;y<302;++y)for(x=0;x<404;++x)guarded[y][x]=0xbeef;
    for(i=0;i<1200;++i){
        game_step(&game,0);
        if(i%40==0)game_render(&game,&guarded[1][2],404);
    }
    /* Renderer clipping also tolerates partially off-canvas sprites. */
    game.enemy[0].active=1;game.enemy[0].x=-4*256;game.enemy[0].y=-3*256;
    game.enemy[1].active=1;game.enemy[1].x=401*256;game.enemy[1].y=299*256;
    game_render(&game,&guarded[1][2],404);
    for(y=0;y<302;++y)for(x=0;x<404;++x)
        if(y==0||y==301||x<2||x>=402)assert(guarded[y][x]==0xbeef);
    game_render(NULL,NULL,0);
    game_render(&game,NULL,400);
    game_render(&game,&guarded[1][2],399);
    puts("render clipping and padded stride: PASS");return 0;
}
