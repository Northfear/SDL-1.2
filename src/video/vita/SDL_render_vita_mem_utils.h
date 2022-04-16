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
#include "SDL_config.h"

#define ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))

void vita_mem_init(size_t size_ram, size_t size_cdram, size_t size_phycont);
void vita_mem_term(void);

void *vita_memalign(size_t alignment, size_t size, VitaMemType type);
void vita_free(void *ptr);

void *vita_gpu_alloc_mapped_aligned(size_t alignment, size_t size, VitaMemType type);
void *vita_gpu_vertex_usse_alloc_mapped(size_t size, unsigned int *usse_offset);
void vita_gpu_vertex_usse_free_mapped(void *addr);
void *vita_gpu_fragment_usse_alloc_mapped(size_t size, unsigned int *usse_offset);
void vita_gpu_fragment_usse_free_mapped(void *addr);

#endif
