/*
 * This file is part of vitaGL
 * Copyright 2017, 2018, 2019, 2020 Rinnegatamante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * mem_utils.c:
 * Utilities for memory management
 */

#include "SDL_render_vita_mem_utils.h"
#include <malloc.h>
#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/clib.h>

#define RAM_ON_DEMAND

// sometimes it's causing some kind of random memory corruption during python init in gemrb
// not 100% sure if it's really caused by newlib mapping tho
//#define MAP_NEWLIB_MEM

static void *mempool_mspace[4] = {NULL, NULL, NULL, NULL}; // mspace creations (VRAM, RAM, PHYCONT RAM, EXTERNAL)
static void *mempool_addr[4] = {NULL, NULL, NULL, NULL}; // addresses of heap memblocks (VRAM, RAM, PHYCONT RAM, EXTERNAL)
static SceUID mempool_id[4] = {0, 0, 0, 0}; // UIDs of heap memblocks (VRAM, RAM, PHYCONT RAM, EXTERNAL)
static size_t mempool_size[4] = {0, 0, 0, 0}; // sizes of heap memlbocks (VRAM, RAM, PHYCONT RAM, EXTERNAL)

static int mempool_initialized = 0;

// VRAM usage setting
uint8_t use_vram_for_usse = 1;


#ifdef RAM_ON_DEMAND
void *vgl_alloc_ram_block(uint32_t size, VitaMemType type) {
    size = ALIGN(size, 4 * 1024);
    SceUID blk = sceKernelAllocMemBlock("rw_mem_blk", type == VITA_MEM_RAM ? SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE : SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, size, NULL);

    if (blk < 0)
        return NULL;

    void *res;
    sceKernelGetMemBlockBase(blk, &res);
    sceGxmMapMemory(res, size, SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE);

    return res;
}
#endif

void vgl_mem_term(void) {
    if (!mempool_initialized)
        return;

    for (int i = 0; i < VITA_MEM_EXTERNAL; i++) {
        if (!mempool_id[i])
            continue;
        sceClibMspaceDestroy(mempool_mspace[i]);
        sceKernelFreeMemBlock(mempool_id[i]);
        mempool_mspace[i] = NULL;
        mempool_addr[i] = NULL;
        mempool_id[i] = 0;
        mempool_size[i] = 0;
    }

    mempool_initialized = 0;
}

void vgl_mem_init(size_t size_ram, size_t size_cdram, size_t size_phycont) {
    if (mempool_initialized)
        vgl_mem_term();

    if (size_ram > 0xC800000) // Vita limits memblocks size to a max of approx. 200 MBs apparently
        size_ram = 0xC800000;

    mempool_size[VITA_MEM_VRAM] = ALIGN(size_cdram, 256 * 1024);
    mempool_size[VITA_MEM_PHYCONT] = ALIGN(size_phycont, 1024 * 1024);
#ifndef RAM_ON_DEMAND
    mempool_size[VITA_MEM_RAM] = ALIGN(size_ram, 4 * 1024);
#endif

    if (mempool_size[VITA_MEM_VRAM])
        mempool_id[VITA_MEM_VRAM] = sceKernelAllocMemBlock("cdram_mempool", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, mempool_size[VITA_MEM_VRAM], NULL);
    if (mempool_size[VITA_MEM_RAM])
        mempool_id[VITA_MEM_RAM] = sceKernelAllocMemBlock("ram_mempool", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, mempool_size[VITA_MEM_RAM], NULL);
    if (mempool_size[VITA_MEM_PHYCONT])
        mempool_id[VITA_MEM_PHYCONT] = sceKernelAllocMemBlock("phycont_mempool", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, mempool_size[VITA_MEM_PHYCONT], NULL);

    for (int i = 0; i < VITA_MEM_EXTERNAL; i++) {
        if (mempool_size[i]) {
            mempool_addr[i] = NULL;
            sceKernelGetMemBlockBase(mempool_id[i], &mempool_addr[i]);

            if (mempool_addr[i]) {
                sceGxmMapMemory(mempool_addr[i], mempool_size[i], SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE);
                mempool_mspace[i] = sceClibMspaceCreate(mempool_addr[i], mempool_size[i]);
            }
        }
    }

#ifdef RAM_ON_DEMAND
    {
        // Getting total available mem
        SceKernelFreeMemorySizeInfo info;
        info.size = sizeof(SceKernelFreeMemorySizeInfo);
        sceKernelGetFreeMemorySize(&info);
        mempool_size[VITA_MEM_RAM] = info.size_user;
    }
#endif

#ifdef MAP_NEWLIB_MEM
    // Mapping newlib heap into sceGxm
    void *dummy = malloc(1);
    free(dummy);

    SceKernelMemBlockInfo info;
    info.size = sizeof(SceKernelMemBlockInfo);
    int err = sceKernelGetMemBlockInfoByAddr(dummy, &info);
    sceGxmMapMemory(info.mappedBase, info.mappedSize, SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE);

    mempool_size[VITA_MEM_EXTERNAL] = info.mappedSize;
    mempool_addr[VITA_MEM_EXTERNAL] = info.mappedBase;
#endif

    mempool_initialized = 1;
}

VitaMemType vgl_mem_get_type_by_addr(void *addr) {
    if (addr >= mempool_addr[VITA_MEM_VRAM] && (addr < mempool_addr[VITA_MEM_VRAM] + mempool_size[VITA_MEM_VRAM]))
        return VITA_MEM_VRAM;
    else if (addr >= mempool_addr[VITA_MEM_PHYCONT] && (addr < mempool_addr[VITA_MEM_PHYCONT] + mempool_size[VITA_MEM_PHYCONT]))
        return VITA_MEM_PHYCONT;
#ifndef RAM_ON_DEMAND
    else if (addr >= mempool_addr[VITA_MEM_RAM] && (addr < mempool_addr[VITA_MEM_RAM] + mempool_size[VITA_MEM_RAM]))
        return VITA_MEM_RAM;
#endif
#ifdef MAP_NEWLIB_MEM
    else if (addr >= mempool_addr[VITA_MEM_EXTERNAL] && (addr < mempool_addr[VITA_MEM_EXTERNAL] + mempool_size[VITA_MEM_EXTERNAL]))
        return VITA_MEM_EXTERNAL;
#endif
#ifdef RAM_ON_DEMAND
    return VITA_MEM_RAM;
#else
    return -1;
#endif
}

void vgl_free(void *ptr) {
    VitaMemType type = vgl_mem_get_type_by_addr(ptr);
    if (mempool_mspace[type])
        sceClibMspaceFree(mempool_mspace[type], ptr);
#ifdef RAM_ON_DEMAND
    else if (type == VITA_MEM_RAM) {
        sceGxmUnmapMemory(ptr);
        sceKernelFreeMemBlock(sceKernelFindMemBlockByAddr(ptr, 0));
    }
#endif
#ifdef MAP_NEWLIB_MEM
    else if (type == VITA_MEM_EXTERNAL)
        free(ptr);
#endif
}

void *vgl_memalign(size_t alignment, size_t size, VitaMemType type) {
    if (mempool_mspace[type])
        return sceClibMspaceMemalign(mempool_mspace[type], alignment, size);
#ifdef RAM_ON_DEMAND
    else if (type == VITA_MEM_RAM || type == VITA_MEM_RAM_CACHED)
        return vgl_alloc_ram_block(size, type);
#endif
#ifdef MAP_NEWLIB_MEM
    else if (type == VITA_MEM_EXTERNAL)
        return memalign(alignment, size);
#endif
    return NULL;
}

void *gpu_alloc_mapped_aligned(size_t alignment, size_t size, VitaMemType type) {
#ifndef RAM_ON_DEMAND
    if (type == VITA_MEM_RAM_CACHED) {
        type = VITA_MEM_RAM;
    }
#endif
    // Allocating requested memblock
    void *res = vgl_memalign(alignment, size, type);
    if (res)
        return res;

    if (type != VITA_MEM_PHYCONT) {
        res = vgl_memalign(alignment, size, VITA_MEM_PHYCONT);
        if (res)
            return res;
    }

    if (type != VITA_MEM_RAM) {
        res = vgl_memalign(alignment, size, VITA_MEM_RAM);
        if (res)
            return res;
    }

    if (type != VITA_MEM_VRAM) {
        res = vgl_memalign(alignment, size, VITA_MEM_VRAM);
        if (res)
            return res;
    }
#ifdef MAP_NEWLIB_MEM
    if (type != VITA_MEM_EXTERNAL) {
        // Internal mempool finished, using newlib mem
        res = vgl_memalign(alignment, size, VITA_MEM_EXTERNAL);
    }
#endif
    return res;
}

void *gpu_vertex_usse_alloc_mapped(size_t size, unsigned int *usse_offset) {
    // Allocating memblock
    void *addr = gpu_alloc_mapped_aligned(4096, size, use_vram_for_usse ? VITA_MEM_VRAM : VITA_MEM_RAM);

    // Mapping memblock into sceGxm as vertex USSE memory
    sceGxmMapVertexUsseMemory(addr, size, usse_offset);

    // Returning memblock starting address
    return addr;
}

void gpu_vertex_usse_free_mapped(void *addr) {
    // Unmapping memblock from sceGxm as vertex USSE memory
    sceGxmUnmapVertexUsseMemory(addr);

#ifdef RAM_ON_DEMAND
    if (!use_vram_for_usse)
    {
        sceKernelFreeMemBlock(sceKernelFindMemBlockByAddr(addr, 0));
        return;
    }
#endif

    // Deallocating memblock
    vgl_free(addr);
}

void *gpu_fragment_usse_alloc_mapped(size_t size, unsigned int *usse_offset) {
    // Allocating memblock
    void *addr = gpu_alloc_mapped_aligned(4096, size, use_vram_for_usse ? VITA_MEM_VRAM : VITA_MEM_RAM);

    // Mapping memblock into sceGxm as fragment USSE memory
    sceGxmMapFragmentUsseMemory(addr, size, usse_offset);

    // Returning memblock starting address
    return addr;
}

void gpu_fragment_usse_free_mapped(void *addr) {
    // Unmapping memblock from sceGxm as fragment USSE memory
    sceGxmUnmapFragmentUsseMemory(addr);

#ifdef RAM_ON_DEMAND
    if (!use_vram_for_usse)
    {
        sceKernelFreeMemBlock(sceKernelFindMemBlockByAddr(addr, 0));
        return;
    }
#endif

    // Deallocating memblock
    vgl_free(addr);
}
