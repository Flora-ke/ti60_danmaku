#ifndef GAME_HARDWARE_MEMORY_H
#define GAME_HARDWARE_MEMORY_H
#include <stdint.h>
#include <stddef.h>
static inline int game_storage_overlap(const void *a,size_t an,const void *b,size_t bn){
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return 1;
    return an&&bn&&x<y+bn&&y<x+an;
}
#endif
