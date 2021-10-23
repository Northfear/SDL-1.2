/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef SDL_RENDER_VITA_GXM_TOOLS_H
#define SDL_RENDER_VITA_GXM_TOOLS_H

#include <psp2/kernel/processmgr.h>
#include <psp2/appmgr.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/kernel/sysmem.h>

#include "SDL_render_vita_gxm_types.h"
#include "SDL.h"

#ifndef SCE_OK
#define SCE_OK 0
#endif

void init_orthographic_matrix(float *m, float left, float right, float bottom, float top, float near, float far);

int gxm_init();
void gxm_finish();

void gxm_wait_rendering_done();

gxm_texture *create_gxm_texture(unsigned int w, unsigned int h, SceGxmTextureFormat format);
void free_gxm_texture(gxm_texture *texture);

void gxm_texture_set_filters(gxm_texture *texture, SceGxmTextureFilter min_filter, SceGxmTextureFilter mag_filter);
void gxm_texture_set_alloc_memblock_type(vglMemType type);
SceGxmTextureFormat gxm_texture_get_format(const gxm_texture *texture);

unsigned int gxm_texture_get_width(const gxm_texture *texture);
unsigned int gxm_texture_get_height(const gxm_texture *texture);
unsigned int gxm_texture_get_stride(const gxm_texture *texture);
void *gxm_texture_get_datap(const gxm_texture *texture);
void *gxm_texture_get_palette(const gxm_texture *texture);

void gxm_draw_screen_texture(gxm_texture *texture, int clear_required);
void gxm_init_texture_scale(const gxm_texture *texture, float x, float y, float x_scale, float y_scale);
void gxm_set_vblank_wait(int enable);
void gxm_set_finish_wait(int enable);
void gxm_render_clear();

#ifdef VITA_HW_ACCEL
void gxm_fill_rect_transfer(gxm_texture *dst, SDL_Rect dstrect, uint32_t color);
void gxm_blit_transfer(gxm_texture *src, SDL_Rect srcrect, gxm_texture *dst, SDL_Rect dstrect, int colorkey_enabled, Uint32 colorkey, Uint32 colorkeyMask);
void gxm_lock_texture(gxm_texture *texture);
#endif

void gxm_minimal_init_for_common_dialog(void);
void gxm_minimal_term_for_common_dialog(void);
void gxm_init_for_common_dialog(void);
void gxm_swap_for_common_dialog(void);
void gxm_term_for_common_dialog(void);

#endif /* SDL_RENDER_VITA_GXM_TOOLS_H */

/* vi: set ts=4 sw=4 expandtab: */
