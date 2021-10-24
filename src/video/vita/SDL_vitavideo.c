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

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_vitavideo.h"
#include "SDL_vitaevents_c.h"
#include "SDL_vitamouse_c.h"
#include "SDL_vitakeyboard_c.h"
#include "SDL_vitatouch.h"

#include "SDL_render_vita_gxm_tools.h"
#include "SDL_render_vita_gxm_types.h"

#define VITAVID_DRIVER_NAME "vita"

typedef struct private_hwdata {
    gxm_texture *texture;
    SDL_Rect dst;
} private_hwdata;

static int vsync = 1;
static int clear_required = 0;

/* Initialization/Query functions */
static int VITA_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **VITA_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *VITA_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int VITA_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void VITA_VideoQuit(_THIS);
static void VITA_DeleteDevice(SDL_VideoDevice *device);

/* Hardware surface functions */
static int VITA_FlipHWSurface(_THIS, SDL_Surface *surface);
static int VITA_AllocHWSurface(_THIS, SDL_Surface *surface);
static int VITA_LockHWSurface(_THIS, SDL_Surface *surface);
static void VITA_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void VITA_FreeHWSurface(_THIS, SDL_Surface *surface);

#ifdef VITA_HW_ACCEL
static int VITA_FillHWRect(_THIS, SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
static int VITA_CheckHWBlit(_THIS, SDL_Surface *src, SDL_Surface *dst);
static int VITA_HWAccelBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
static int VITA_SetHWAlpha(_THIS, SDL_Surface *surface, Uint8 alpha);
static int VITA_SetHWColorKey(_THIS, SDL_Surface *src, Uint32 key);
#endif

/* etc. */
static void VITA_UpdateRects(_THIS, int numrects, SDL_Rect *rects);

/* VITA driver bootstrap functions */
static int VITA_Available(void)
{
    return 1;
}

static SDL_VideoDevice *VITA_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
    if ( device ) {
        SDL_memset(device, 0, (sizeof *device));
        device->hidden = (struct SDL_PrivateVideoData *)
                SDL_malloc((sizeof *device->hidden));
    }
    if ( (device == NULL) || (device->hidden == NULL) ) {
        SDL_OutOfMemory();
        if ( device ) {
            SDL_free(device);
        }
        return(0);
    }
    SDL_memset(device->hidden, 0, (sizeof *device->hidden));

    /* Set the function pointers */
    device->VideoInit = VITA_VideoInit;
    device->ListModes = VITA_ListModes;
    device->SetVideoMode = VITA_SetVideoMode;
    device->CreateYUVOverlay = NULL;
    device->SetColors = VITA_SetColors;
    device->UpdateRects = VITA_UpdateRects;
    device->VideoQuit = VITA_VideoQuit;
    device->AllocHWSurface = VITA_AllocHWSurface;
#ifdef VITA_HW_ACCEL
    device->CheckHWBlit = VITA_CheckHWBlit;
    device->FillHWRect = VITA_FillHWRect;
    device->SetHWColorKey = VITA_SetHWColorKey;
    device->SetHWAlpha = VITA_SetHWAlpha;
#else
    device->CheckHWBlit = NULL;
    device->FillHWRect = NULL;
    device->SetHWColorKey = NULL;
    device->SetHWAlpha = NULL;
#endif
    device->LockHWSurface = VITA_LockHWSurface;
    device->UnlockHWSurface = VITA_UnlockHWSurface;
    device->FlipHWSurface = VITA_FlipHWSurface;
    device->FreeHWSurface = VITA_FreeHWSurface;
    device->SetCaption = NULL;
    device->SetIcon = NULL;
    device->IconifyWindow = NULL;
    device->GrabInput = NULL;
    device->GetWMInfo = NULL;
    device->InitOSKeymap = VITA_InitOSKeymap;
    device->PumpEvents = VITA_PumpEvents;

    device->free = VITA_DeleteDevice;

    return device;
}

VideoBootStrap VITA_bootstrap = {
    VITAVID_DRIVER_NAME, "SDL vita video driver",
    VITA_Available, VITA_CreateDevice
};

static void VITA_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device->hidden);
    SDL_free(device);
}

int VITA_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
    // VITA_BLIT_HW_A support is limited to full transparency only
    this->info.hw_available = 1;
    this->info.blit_hw = VITA_BLIT_HW;
    this->info.blit_hw_CC = VITA_BLIT_HW;
    this->info.blit_hw_A = VITA_BLIT_HW_A;
    this->info.blit_sw = 0;
    this->info.blit_sw_CC = 0;
    this->info.blit_sw_A = 0;
    this->info.blit_fill = VITA_FILL_HW;

    if (gxm_init() != 0)
    {
        return -1;
    }
    gxm_set_vblank_wait(vsync);

    vformat->BitsPerPixel = 16;
    vformat->BytesPerPixel = 2;
    vformat->Rmask = 0xF800;
    vformat->Gmask = 0x07E0;
    vformat->Bmask = 0x001F;
    vformat->Amask = 0x0000;

    VITA_InitKeyboard();
    VITA_InitMouse();
    VITA_InitTouch();

    return(0);
}

SDL_Rect **VITA_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
    static SDL_Rect VITA_Rects[] = {
        {0, 0, 320, 200},
        {0, 0, 480, 272},
        {0, 0, 640, 400},
        {0, 0, 640, 480},
        {0, 0, 960, 544},
    };
    static SDL_Rect *VITA_modes[] = {
        &VITA_Rects[0],
        &VITA_Rects[1],
        &VITA_Rects[2],
        &VITA_Rects[3],
        &VITA_Rects[4],
        NULL
    };
    SDL_Rect **modes = VITA_modes;

    switch(format->BitsPerPixel)
    {
        case 8:
        case 15:
        case 16:
        case 24:
        case 32:
        return modes;

        default:
        return (SDL_Rect **) -1;
    }
}

SDL_Surface *VITA_SetVideoMode(_THIS, SDL_Surface *current,
                int width, int height, int bpp, Uint32 flags)
{
    switch(bpp)
    {
        case 8:
            if (!SDL_ReallocFormat(current, 8, 0, 0, 0, 0))
            {
                SDL_SetError("Couldn't allocate new pixel format for requested mode");
                return(NULL);
            }
        break;

        case 15:
            if (!SDL_ReallocFormat(current, 15, 0x7C00, 0x03E0, 0x001F, 0x0000))
            {
                SDL_SetError("Couldn't allocate new pixel format for requested mode");
                return(NULL);
            }
        break;

        case 16:
        break;

        case 24:
            if (!SDL_ReallocFormat(current, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0x000000))
            {
                SDL_SetError("Couldn't allocate new pixel format for requested mode");
                return(NULL);
            }
        break;

        case 32:
            if (!SDL_ReallocFormat(current, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000))
            {
                SDL_SetError("Couldn't allocate new pixel format for requested mode");
                return(NULL);
            }
        break;

        default:
            if (!SDL_ReallocFormat(current, 16, 0xF800, 0x07E0, 0x001F, 0x0000))
            {
                SDL_SetError("Couldn't allocate new pixel format for requested mode");
                return(NULL);
            }
        break;
    }

    current->flags = flags | SDL_FULLSCREEN | SDL_DOUBLEBUF;
    current->w = width;
    current->h = height;

    // remove old texture first to avoid crashes during resolution change
    if(current->hwdata != NULL)
    {
        VITA_FreeHWSurface(this, current);
    }

    VITA_AllocHWSurface(this, current);

    // clear and center non-fullscreen screen surfaces by default
    if (width != SCREEN_W || height != SCREEN_H)
    {
        clear_required = 1;
        current->hwdata->dst.x = (SCREEN_W - width) / 2;
        current->hwdata->dst.y = (SCREEN_H - height) / 2;
    }
    else
    {
        clear_required = 0;
    }

    gxm_init_texture_scale(
        current->hwdata->texture,
        current->hwdata->dst.x, current->hwdata->dst.y,
        (float)current->hwdata->dst.w/(float)current->w,
        (float)current->hwdata->dst.h/(float)current->h);

    return(current);
}

static int VITA_AllocHWSurface(_THIS, SDL_Surface *surface)
{
    surface->hwdata = (private_hwdata*) SDL_malloc (sizeof (private_hwdata));
    if (surface->hwdata == NULL)
    {
        SDL_OutOfMemory();
        return -1;
    }
    SDL_memset (surface->hwdata, 0, sizeof(private_hwdata));

    // set initial texture destination
    surface->hwdata->dst.x = 0;
    surface->hwdata->dst.y = 0;
    surface->hwdata->dst.w = surface->w;
    surface->hwdata->dst.h = surface->h;

    switch(surface->format->BitsPerPixel)
    {
        case 8:
            surface->hwdata->texture =
                create_gxm_texture(surface->w, surface->h, SCE_GXM_TEXTURE_FORMAT_P8_1BGR);
        break;

        case 15:
            surface->hwdata->texture =
                create_gxm_texture(surface->w, surface->h, SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ARGB);
        break;

        case 16:
            surface->hwdata->texture =
                create_gxm_texture(surface->w, surface->h, SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB);
        break;

        case 24:
            surface->hwdata->texture =
                create_gxm_texture(surface->w, surface->h, SCE_GXM_TEXTURE_FORMAT_U8U8U8_BGR);
        break;

        case 32:
            surface->hwdata->texture =
                create_gxm_texture(surface->w, surface->h, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
        break;

        default:
            SDL_SetError("unsupported BitsPerPixel: %i\n", surface->format->BitsPerPixel);
        return -1;
    }

    if (!surface->hwdata->texture) {
        SDL_free(surface->hwdata);
        SDL_OutOfMemory();
        return -1;
    }

    surface->pixels = gxm_texture_get_datap(surface->hwdata->texture);
    surface->pitch = gxm_texture_get_stride(surface->hwdata->texture);
    surface->flags |= SDL_HWSURFACE;

    return(0);
}

static void VITA_FreeHWSurface(_THIS, SDL_Surface *surface)
{
    if (surface->hwdata != NULL)
    {
        // it's possible to speed thing up with delayed removal
        gxm_wait_rendering_done();
        free_gxm_texture(surface->hwdata->texture);
        SDL_free(surface->hwdata);
        surface->hwdata = NULL;
        surface->pixels = NULL;
    }
}

static int VITA_LockHWSurface(_THIS, SDL_Surface *surface)
{
#ifdef VITA_HW_ACCEL
    // texture might be accessed by gxm right now
    // so we should finish rendering before messing it up
    gxm_lock_texture(surface->hwdata->texture);
#endif
    return(0);
}

static void VITA_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
    return;
}

static int VITA_FlipHWSurface(_THIS, SDL_Surface *surface)
{
    gxm_draw_screen_texture(surface->hwdata->texture, clear_required);
    return(0);
}

#ifdef VITA_HW_ACCEL
static int VITA_FillHWRect(_THIS, SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color)
{
    SDL_Rect dst_rect;
    gxm_texture *dst_texture = dst->hwdata->texture;

    if (dstrect == NULL)
    {
        dst_rect.x = 0;
        dst_rect.y = 0;
        dst_rect.w = gxm_texture_get_width(dst_texture);
        dst_rect.h = gxm_texture_get_height(dst_texture);
    }
    else
    {
        dst_rect = *dstrect;
    }

    // fallback to SW fill for smaller fills
    const int min_blit_size = 1024;
    if (dst_rect.w * dst_rect.h <= min_blit_size && dst->format->BytesPerPixel != 3)
    {
        Uint8 *row = (Uint8 *)dst->pixels+dstrect->y*dst->pitch+
                dstrect->x*dst->format->BytesPerPixel;

        //lock before SW blit
        gxm_lock_texture(dst_texture);

        void FillRect8ARMNEONAsm(int32_t w, int32_t h, uint8_t *dst, int32_t dst_stride, uint8_t src);
        void FillRect16ARMNEONAsm(int32_t w, int32_t h, uint16_t *dst, int32_t dst_stride, uint16_t src);
        void FillRect32ARMNEONAsm(int32_t w, int32_t h, uint32_t *dst, int32_t dst_stride, uint32_t src);

        switch (dst->format->BytesPerPixel) {
        case 1:
            FillRect8ARMNEONAsm(dstrect->w, dstrect->h, (uint8_t *) row, dst->pitch >> 0, color);
            break;
        case 2:
            FillRect16ARMNEONAsm(dstrect->w, dstrect->h, (uint16_t *) row, dst->pitch >> 1, color);
            break;
        case 4:
            FillRect32ARMNEONAsm(dstrect->w, dstrect->h, (uint32_t *) row, dst->pitch >> 2, color);
            break;
        }
        return(0);
    }

    // sceGxmTransferFill is using U8U8U8U8_ABGR color format, so we should fix it for 15/16 bit surfaces
    if (dst->format->BytesPerPixel == 2)
    {
        Uint8 r;
        Uint8 g;
        Uint8 b;
        Uint8 a;
        SDL_GetRGBA(color, dst->format, &r, &g, &b, &a);
        color = (a << 24) | (r << 16) | (g << 8) | (b << 0);
        //color = (a << 24) | (b << 16) | (g << 8) | (r << 0);
    }

    gxm_fill_rect_transfer(dst_texture, *dstrect, color);
    return(0);
}

static int VITA_CheckHWBlit(_THIS, SDL_Surface *src, SDL_Surface *dst)
{
    int accelerated;

    // Set initial acceleration on
    src->flags |= SDL_HWACCEL;

    // Set the surface attributes
    if ((src->flags & SDL_SRCALPHA) == SDL_SRCALPHA)
    {
        if (!this->info.blit_hw_A)
        {
            src->flags &= ~SDL_HWACCEL;
        }
    }
    if ((src->flags & SDL_SRCCOLORKEY) == SDL_SRCCOLORKEY)
    {
        if (!this->info.blit_hw_CC)
        {
            src->flags &= ~SDL_HWACCEL;
        }
    }

    // Check to see if final surface blit is accelerated
    accelerated = !!(src->flags & SDL_HWACCEL);
    if (accelerated)
    {
        src->map->hw_blit = VITA_HWAccelBlit;
    }
    return(accelerated);
}

static int VITA_HWAccelBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
    gxm_texture *src_texture = src->hwdata->texture;
    gxm_texture *dst_texture = dst->hwdata->texture;
    SDL_Rect src_rect;
    SDL_Rect dst_rect;

    if (srcrect == NULL)
    {
        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.w = gxm_texture_get_width(src_texture);
        src_rect.h = gxm_texture_get_height(src_texture);
    }
    else
    {
        src_rect = *srcrect;
    }

    if (dstrect == NULL)
    {
        dst_rect.x = 0;
        dst_rect.y = 0;
    }
    else
    {
        dst_rect = *dstrect;
    }

    int alpha_blit = ((src->flags & SDL_SRCALPHA) == SDL_SRCALPHA) ? 1 : 0;
    int colorkey_blit = ((src->flags & SDL_SRCCOLORKEY) == SDL_SRCCOLORKEY) ? 1 : 0;

    if (alpha_blit)
    {
        if (src->format->alpha != SDL_ALPHA_TRANSPARENT)
        {
            gxm_blit_transfer(src_texture, src_rect, dst_texture, dst_rect, alpha_blit, 0, src->format->Amask);
        }
    }
    else
    {
        Uint32 colorkey_mask;
        if (src->format->BytesPerPixel == 1)
            colorkey_mask = 0xFF;
        else
            colorkey_mask = src->format->Amask | src->format->Bmask | src->format->Gmask | src->format->Rmask;

        gxm_blit_transfer(src_texture, src_rect, dst_texture, dst_rect, colorkey_blit, src->format->colorkey, colorkey_mask);
    }

    return(0);
}

static int VITA_SetHWAlpha(_THIS, SDL_Surface *surface, Uint8 alpha)
{
    return 0;
}

static int VITA_SetHWColorKey(_THIS, SDL_Surface *src, Uint32 key)
{
    return 0;
}
#endif

static void VITA_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
}

int VITA_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
    void *palette_ptr = gxm_texture_get_palette(this->screen->hwdata->texture);
    if (palette_ptr != NULL) {
        SDL_memcpy(palette_ptr + sizeof(uint32_t) * firstcolor, colors, sizeof(uint32_t) * ncolors);
        return(1);
    }
    return(0);
}

void VITA_VideoQuit(_THIS)
{
    if (this->screen->hwdata != NULL)
    {
        VITA_FreeHWSurface(this, this->screen);
    }
    gxm_finish();
}

// custom vita function for centering/scaling main screen surface (texture)
void SDL_VITA_SetVideoModeScaling(int x, int y, float w, float h)
{
    SDL_Surface *surface = SDL_VideoSurface;

    if (surface != NULL && surface->hwdata != NULL)
    {
        surface->hwdata->dst.x = x;
        surface->hwdata->dst.y = y;
        surface->hwdata->dst.w = w;
        surface->hwdata->dst.h = h;

        gxm_init_texture_scale(
            surface->hwdata->texture,
            surface->hwdata->dst.x, surface->hwdata->dst.y,
            (float)surface->hwdata->dst.w/(float)surface->w,
            (float)surface->hwdata->dst.h/(float)surface->h);

        if (w != SCREEN_W || h != SCREEN_H)
            clear_required = 1;
        else
            clear_required = 0;
    }
}

// custom vita function for setting the texture filter to nearest or bilinear
void SDL_VITA_SetVideoModeBilinear(int enable_bilinear)
{
    SDL_Surface *surface = SDL_VideoSurface;
    
    if (surface != NULL && surface->hwdata != NULL)
    {
        if (enable_bilinear)
        {
            //reduce pixelation by setting bilinear filtering
            //for magnification
            //(first one is minimization filter,
            //second one is magnification filter)
            gxm_texture_set_filters(surface->hwdata->texture,
                SCE_GXM_TEXTURE_FILTER_LINEAR,
                SCE_GXM_TEXTURE_FILTER_LINEAR);
        }
        else
        {
            gxm_texture_set_filters(surface->hwdata->texture,
                SCE_GXM_TEXTURE_FILTER_POINT,
                SCE_GXM_TEXTURE_FILTER_POINT);
        }
    }
}	

// custom vita function for vsync
void SDL_VITA_SetVideoModeSync(int enable_vsync)
{
    vsync = enable_vsync;
    gxm_set_vblank_wait(vsync);
}

// custom vita function for setting mem type for new hw texture allocations
void SDL_VITA_SetTextureAllocMemblockType(vglMemType type)
{
    gxm_texture_set_alloc_memblock_type(type);
}

// custom vita function that returns main surface rect and scaled rect (used in touch emulation)
void SDL_VITA_GetSurfaceRect(SDL_Rect *surfaceRect, SDL_Rect *scaledRect)
{
    SDL_Surface *surface = SDL_VideoSurface;

    surfaceRect->x = 0;
    surfaceRect->y = 0;
    surfaceRect->w = SCREEN_W;
    surfaceRect->h = SCREEN_H;
    scaledRect->x = 0;
    scaledRect->y = 0;
    scaledRect->w = SCREEN_W;
    scaledRect->h = SCREEN_H;

    if (surface != NULL && surface->hwdata != NULL)
    {
        surfaceRect->x = 0;
        surfaceRect->y = 0;
        surfaceRect->w = surface->w;
        surfaceRect->h = surface->h;
        *scaledRect = surface->hwdata->dst;
    }
}
