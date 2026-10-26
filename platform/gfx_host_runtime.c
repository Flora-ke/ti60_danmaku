/* Host-only implementations of the existing gfx runtime hooks. No renderer
 * lives here. The Sapphire build instead links software/platform/sapphire/runtime.c. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "gfx_runtime.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif
size_t gfx_test_alloc_calls, gfx_test_free_calls;
size_t gfx_host_live_bytes, gfx_host_peak_bytes;
size_t gfx_test_fail_after=SIZE_MAX;
typedef struct {void *base;size_t bytes;} Allocation;
static gfx_error allocate(size_t alignment,size_t bytes,void **out){
    void *base;uintptr_t p;Allocation allocation;
    if(!out)return GFX_INVALID;
    *out=NULL;if(!bytes||alignment<sizeof(void*)||(alignment&(alignment-1)))return GFX_INVALID;
    if(bytes>SIZE_MAX-sizeof(Allocation)-(alignment-1))return GFX_OVERFLOW;
    ++gfx_test_alloc_calls;
    if(gfx_test_alloc_calls>gfx_test_fail_after)return GFX_NO_MEMORY;
    base=malloc(bytes+sizeof(Allocation)+alignment-1);if(!base)return GFX_NO_MEMORY;
    p=((uintptr_t)base+sizeof(Allocation)+alignment-1)&~(uintptr_t)(alignment-1);
    allocation=(Allocation){base,bytes};memcpy((void *)(p-sizeof(Allocation)),&allocation,sizeof(allocation));
    gfx_host_live_bytes+=bytes;if(gfx_host_live_bytes>gfx_host_peak_bytes)gfx_host_peak_bytes=gfx_host_live_bytes;
    *out=(void *)p;return GFX_OK;
}
gfx_error gfx_alloc(size_t bytes,void **out){return allocate(_Alignof(max_align_t),bytes,out);}
gfx_error gfx_aligned_alloc(size_t alignment,size_t bytes,void **out){return allocate(alignment,bytes,out);}
void gfx_free(void *ptr){
    Allocation allocation;if(!ptr)return;
    memcpy(&allocation,(unsigned char *)ptr-sizeof(allocation),sizeof(allocation));
    ++gfx_test_free_calls;gfx_host_live_bytes-=allocation.bytes;free(allocation.base);
}
void gfx_aligned_free(void *ptr){gfx_free(ptr);}
void gfx_log(const char *message){if(message)fprintf(stderr,"[gfx] %s\n",message);}
gfx_error gfx_clock_now(uint64_t *ticks){
    if(!ticks)return GFX_INVALID;
    *ticks=0;
#ifdef _WIN32
    {LARGE_INTEGER t;if(!QueryPerformanceCounter(&t)||t.QuadPart<0)return GFX_PLATFORM_ERROR;*ticks=(uint64_t)t.QuadPart;}
#else
    {struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return GFX_PLATFORM_ERROR;
     *ticks=(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;}
#endif
    return GFX_OK;
}
gfx_error gfx_clock_frequency(uint64_t *frequency){
    if(!frequency)return GFX_INVALID;
#ifdef _WIN32
    {LARGE_INTEGER f;*frequency=0;if(!QueryPerformanceFrequency(&f)||f.QuadPart<=0)return GFX_PLATFORM_ERROR;*frequency=(uint64_t)f.QuadPart;}
#else
    *frequency=UINT64_C(1000000000);
#endif
    return GFX_OK;
}
