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

#ifndef SDL_RENDER_VITA_GXM_TYPES_H
#define SDL_RENDER_VITA_GXM_TYPES_H

#include <psp2/kernel/processmgr.h>
#include <psp2/appmgr.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/kernel/sysmem.h>


#define VITA_GXM_SCREEN_WIDTH     960
#define VITA_GXM_SCREEN_HEIGHT    544
#define VITA_GXM_SCREEN_STRIDE    960

#define VITA_GXM_COLOR_FORMAT    SCE_GXM_COLOR_FORMAT_A8B8G8R8
#define VITA_GXM_PIXEL_FORMAT    SCE_DISPLAY_PIXELFORMAT_A8B8G8R8

#define VITA_GXM_BUFFERS          3
#define VITA_GXM_PENDING_SWAPS    2
#ifdef VITA_HW_ACCEL
#define VITA_GXM_POOL_SIZE        1 * 1024 * 1024
#endif


typedef struct
{
    void     *address;
    int      vblank_wait;
} VITA_GXM_DisplayData;

typedef struct clear_vertex {
    float x;
    float y;
} clear_vertex;

typedef struct texture_vertex {
    float x;
    float y;
    float z;
    float u;
    float v;
} texture_vertex;

typedef struct gxm_texture {
    SceGxmTexture gxm_tex;
    void *data;
    void *palette;
#ifdef VITA_HW_ACCEL
    SceGxmRenderTarget *gxm_rendertarget;
    SceGxmColorSurface gxm_colorsurface;
    SceGxmDepthStencilSurface gxm_depthstencil;
    void *depth;
    int notification_id;
    SceGxmNotification fragment_notif;
#endif
} gxm_texture;

typedef struct fragment_programs {
    SceGxmFragmentProgram *texture;
} fragment_programs;

typedef struct blend_fragment_programs {
    fragment_programs blend_mode_none;
#ifdef VITA_HW_ACCEL
    fragment_programs blend_mode_blend;
#endif
} blend_fragment_programs;

typedef struct
{
    VITA_GXM_DisplayData displayData;

    void *vdmRingBuffer;
    void *vertexRingBuffer;
    void *fragmentRingBuffer;
    void *fragmentUsseRingBuffer;
    SceGxmContextParams contextParams;
    SceGxmContext *gxm_context;
    SceGxmRenderTarget *renderTarget;
    void *displayBufferData[VITA_GXM_BUFFERS];
    SceGxmColorSurface displaySurface[VITA_GXM_BUFFERS];
    SceGxmSyncObject *displayBufferSync[VITA_GXM_BUFFERS];

    SceGxmDepthStencilSurface depthSurface;
    void *depthBufferData;
    void *stencilBufferData;

    unsigned int backBufferIndex;
    unsigned int frontBufferIndex;

    texture_vertex *screenVertices;

    float ortho_matrix[4*4];

    SceGxmVertexProgram *textureVertexProgram;
    SceGxmFragmentProgram *textureFragmentProgram;
    SceGxmVertexProgram *clearVertexProgram;
    SceGxmFragmentProgram *clearFragmentProgram;

    SceGxmProgramParameter *textureWvpParam;
    SceGxmProgramParameter *clearClearColorParam;

    SceGxmShaderPatcher *shaderPatcher;

    SceGxmShaderPatcherId textureVertexProgramId;
    SceGxmShaderPatcherId textureFragmentProgramId;
    SceGxmShaderPatcherId clearVertexProgramId;
    SceGxmShaderPatcherId clearFragmentProgramId;

    void *patcherBuffer;
    void *patcherVertexUsse;
    void *patcherFragmentUsse;

    uint16_t *linearIndices;
    clear_vertex *clearVertices;
    blend_fragment_programs blendFragmentPrograms;
#ifdef VITA_HW_ACCEL
    void* pool_addr[2];
    unsigned int pool_index;
    unsigned int current_pool;
    SceGxmRenderTarget *lastRenderTarget;
    gxm_texture *lastTargetTexture;
#endif
} VITA_GXM_RenderData;


#endif /* SDL_RENDER_VITA_GXM_TYPES_H */

/* vi: set ts=4 sw=4 expandtab: */
