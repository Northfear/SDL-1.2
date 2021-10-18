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
 * mem_utils.h:
 * Header file for the memory management utilities exposed by mem_utils.c
 */

#ifndef _MEM_UTILS_H_
#define _MEM_UTILS_H_

#include <stdlib.h>

#define ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))

typedef enum {
	VGL_MEM_VRAM, // CDRAM
	VGL_MEM_RAM, // USER_RW RAM
	VGL_MEM_SLOW, // PHYCONT_USER_RW RAM
	VGL_MEM_EXTERNAL, // newlib mem
	VGL_MEM_ALL
} vglMemType;

void vgl_mem_init(size_t size_ram, size_t size_cdram, size_t size_phycont);
void vgl_mem_term(void);
size_t vgl_mem_get_free_space(vglMemType type);
size_t vgl_mem_get_total_space(vglMemType type);

size_t vgl_malloc_usable_size(void *ptr);
void *vgl_malloc(size_t size, vglMemType type);
void *vgl_calloc(size_t num, size_t size, vglMemType type);
void *vgl_memalign(size_t alignment, size_t size, vglMemType type);
void *vgl_realloc(void *ptr, size_t size);
void vgl_free(void *ptr);

void *gpu_alloc_mapped_aligned(size_t alignment, size_t size, vglMemType type);
void *gpu_alloc_mapped(size_t size, vglMemType type);
void *gpu_vertex_usse_alloc_mapped(size_t size, unsigned int *usse_offset);
void gpu_vertex_usse_free_mapped(void *addr);
void *gpu_fragment_usse_alloc_mapped(size_t size, unsigned int *usse_offset);
void gpu_fragment_usse_free_mapped(void *addr);

#endif
