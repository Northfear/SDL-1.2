/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#ifndef _SDL_vitavideo_h
#define _SDL_vitavideo_h

#include "../SDL_sysvideo.h"

#include "SDL_render_vita_gxm_types.h"

#if SDL_VIDEO_OPENGL_VITAGL
#include "vitaGL.h"

#define MEMORY_VITAGL_THRESHOLD 12 * 1024 * 1024
#endif

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_VideoDevice *this

#define SCREEN_W 960
#define SCREEN_H 544

void SDL_VITA_GetSurfaceRect(SDL_Rect *surfaceRect, SDL_Rect *scaledRect);

/* Private display data */

struct SDL_PrivateVideoData {
    SDL_Rect dst;
};

#endif /* _SDL_vitavideo_h */
