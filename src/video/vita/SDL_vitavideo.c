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

/* VITA SDL video driver implementation; this is just enough to make an
 *  SDL-based application THINK it's got a working video driver, for
 *  applications that call SDL_Init(SDL_INIT_VIDEO) when they don't need it,
 *  and also for use as a collection of stubs when porting SDL to a new
 *  platform for which you haven't yet written a valid video driver.
 *
 * This is also a great way to determine bottlenecks: if you think that SDL
 *  is a performance problem for a given platform, enable this driver, and
 *  then see if your application runs faster without video overhead.
 *
 * Initial work by Ryan C. Gordon (icculus@icculus.org). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "DUMMY" by Sam Lantinga.
 */

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
    device->CheckHWBlit = NULL;
    device->FillHWRect = NULL;
    device->SetHWColorKey = NULL;
    device->SetHWAlpha = NULL;
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
        {0, 0, 960, 544},
    };
    static SDL_Rect *VITA_modes[] = {
        &VITA_Rects[0],
        &VITA_Rects[1],
        &VITA_Rects[2],
        &VITA_Rects[3],
        NULL
    };
    SDL_Rect **modes = VITA_modes;

    switch(format->BitsPerPixel)
    {
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
        case 16:
        break;

        case 24:
            if (!SDL_ReallocFormat(current, 24, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000))
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
        case 16:
            surface->hwdata->texture =
                create_gxm_texture(surface->w, surface->h, SCE_GXM_TEXTURE_FORMAT_R5G6B5);
        break;

        case 24:
            surface->hwdata->texture =
                create_gxm_texture(surface->w, surface->h, SCE_GXM_TEXTURE_FORMAT_U8U8U8_RGB);
        break;

        case 32:
            surface->hwdata->texture =
                create_gxm_texture(surface->w, surface->h, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
        break;

        default:
            SDL_SetError("unsupported BitsPerPixel: %i\n", surface->format->BitsPerPixel);
        return -1;
    }

    surface->pixels = gxm_texture_get_datap(surface->hwdata->texture);
    surface->pitch = gxm_texture_get_stride(surface->hwdata->texture);
    // Don't force SDL_HWSURFACE. Screen surface still works as SDL_SWSURFACE (but may require sceGxmFinish on flip)
    // Mixing SDL_HWSURFACE and SDL_SWSURFACE drops fps by 10% or so
    // Not sure if there's even a point of having anything as SDL_HWSURFACE
    // UPD: Yeah, there's a point is some cases (like weird bugs with 32 bits with SW or HW only surfaces in gemrb)
    //surface->flags |= SDL_HWSURFACE;

    return(0);
}

static void VITA_FreeHWSurface(_THIS, SDL_Surface *surface)
{
    if (surface->hwdata != NULL)
    {
        gxm_wait_rendering_done();
        free_gxm_texture(surface->hwdata->texture);
        SDL_free(surface->hwdata);
        surface->hwdata = NULL;
        surface->pixels = NULL;
    }
}

static int VITA_LockHWSurface(_THIS, SDL_Surface *surface)
{
    return(0);
}

static void VITA_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
    return;
}

static int VITA_FlipHWSurface(_THIS, SDL_Surface *surface)
{
    gxm_start_drawing();
    if (clear_required)
    {
        // clear to avoid leftovers from previous frames (when rendering non native resolutions)
        gxm_render_clear();
    }
    gxm_draw_texture(surface->hwdata->texture);
    gxm_end_drawing();
    gxm_swap_buffers();
}

static void VITA_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
}

int VITA_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
    return(1);
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

// custom vita function for doing sceGxmFinish on Flip (may be required in case of visual bugs)
void SDL_VITA_SetWaitGxmFinish(int gxm_wait)
{
    gxm_set_finish_wait(gxm_wait);
}

// custom vita function for setting mem type for new hw texture allocations
void SDL_VITA_SetTextureAllocMemblockType(SceKernelMemBlockType type)
{
    gxm_texture_set_alloc_memblock_type(type);
}

// custom psp2 function that returns main surface rect and scaled rect (used in touch emulation)
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
