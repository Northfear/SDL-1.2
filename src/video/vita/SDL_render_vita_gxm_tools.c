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

#include <psp2/kernel/processmgr.h>
#include <psp2/appmgr.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/common_dialog.h>
#include <psp2/message_dialog.h>

#include "SDL_error.h"

#include "SDL_render_vita_gxm_tools.h"
#include "SDL_render_vita_gxm_shaders.h"
#include "SDL_render_vita_mem_utils.h"

#define MAX_SCENES_PER_FRAME 8

VITA_GXM_RenderData *data;
static VitaMemType textureMemBlockType = VITA_MEM_VRAM;
volatile unsigned int *notificationMem;
SceGxmNotification flipFragmentNotif;
gxm_texture *lastScreenTexture = NULL;
uint8_t use_vram = 1;

#ifdef VITA_HW_ACCEL
#define NOTIF_NUM 512
int notification_busy[NOTIF_NUM];
int notification_limit_reached = 0;
#endif


void init_orthographic_matrix(float *m, float left, float right, float bottom, float top, float near, float far)
{
    m[0x0] = 2.0f/(right-left);
    m[0x4] = 0.0f;
    m[0x8] = 0.0f;
    m[0xC] = -(right+left)/(right-left);

    m[0x1] = 0.0f;
    m[0x5] = 2.0f/(top-bottom);
    m[0x9] = 0.0f;
    m[0xD] = -(top+bottom)/(top-bottom);

    m[0x2] = 0.0f;
    m[0x6] = 0.0f;
    m[0xA] = -2.0f/(far-near);
    m[0xE] = (far+near)/(far-near);

    m[0x3] = 0.0f;
    m[0x7] = 0.0f;
    m[0xB] = 0.0f;
    m[0xF] = 1.0f;
}

static void* patcher_host_alloc(void *user_data, unsigned int size)
{
    (void)user_data;
    void *mem = SDL_malloc(size);
    return mem;
}

static void patcher_host_free(void *user_data, void *mem)
{
    (void)user_data;
    SDL_free(mem);
}

static int tex_format_to_bytespp(SceGxmTextureFormat format)
{
    switch (format & 0x9f000000U) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
        return 1;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
        return 2;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
        return 3;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
    default:
        return 4;
    }
}

static void display_callback(const void *callback_data)
{
    SceDisplayFrameBuf framebuf;
    const VITA_GXM_DisplayData *display_data = (const VITA_GXM_DisplayData *)callback_data;

    SDL_memset(&framebuf, 0x00, sizeof(SceDisplayFrameBuf));
    framebuf.size        = sizeof(SceDisplayFrameBuf);
    framebuf.base        = display_data->address;
    framebuf.pitch       = VITA_GXM_SCREEN_STRIDE;
    framebuf.pixelformat = VITA_GXM_PIXEL_FORMAT;
    framebuf.width       = VITA_GXM_SCREEN_WIDTH;
    framebuf.height      = VITA_GXM_SCREEN_HEIGHT;
    sceDisplaySetFrameBuf(&framebuf, SCE_DISPLAY_SETBUF_NEXTFRAME);

    if (display_data->vblank_wait) {
        sceDisplayWaitVblankStart();
    }
}

int gxm_init()
{
    data = (VITA_GXM_RenderData *) SDL_calloc(1, sizeof(VITA_GXM_RenderData));

    unsigned int i, x, y;
    int err;

    SceGxmInitializeParams initializeParams;
    SDL_memset(&initializeParams, 0, sizeof(SceGxmInitializeParams));
    initializeParams.flags                          = 0;
    initializeParams.displayQueueMaxPendingCount    = VITA_GXM_PENDING_SWAPS;
    initializeParams.displayQueueCallback           = display_callback;
    initializeParams.displayQueueCallbackDataSize   = sizeof(VITA_GXM_DisplayData);
    initializeParams.parameterBufferSize            = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;

    err = sceGxmInitialize(&initializeParams);

    if (err != SCE_OK) {
        SDL_SetError("gxm init failed: %d\n", err);
        return err;
    }

    SceKernelFreeMemorySizeInfo info;
    info.size = sizeof(SceKernelFreeMemorySizeInfo);
    sceKernelGetFreeMemorySize(&info);
    int ram_threshold = 0x1000000;
    int cdram_threshold = 0 * 256 * 1024;
    int phycont_threshold = 0 * 1024 * 1024;
    size_t ram_size = info.size_user > ram_threshold ? info.size_user - ram_threshold : info.size_user;
    size_t cdram_size = info.size_cdram > cdram_threshold ? info.size_cdram - cdram_threshold : 0;
    size_t phycont_size = info.size_phycont > phycont_threshold ? info.size_phycont - phycont_threshold : 0;

    vgl_mem_init(ram_size, cdram_size, phycont_size);

    // allocate ring buffer memory using default sizes
    data->vdmRingBuffer = gpu_alloc_mapped_aligned(4096, SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE, use_vram ? VITA_MEM_VRAM : VITA_MEM_RAM);
    data->vertexRingBuffer = gpu_alloc_mapped_aligned(4096, SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE, use_vram ? VITA_MEM_VRAM : VITA_MEM_RAM);
    data->fragmentRingBuffer = gpu_alloc_mapped_aligned(4096, SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE, use_vram ? VITA_MEM_VRAM : VITA_MEM_RAM);

    unsigned int fragmentUsseRingBufferOffset;
    data->fragmentUsseRingBuffer = gpu_fragment_usse_alloc_mapped(SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE, &fragmentUsseRingBufferOffset);

    SDL_memset(&data->contextParams, 0, sizeof(SceGxmContextParams));
    data->contextParams.hostMem                       = SDL_malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
    data->contextParams.hostMemSize                   = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
    data->contextParams.vdmRingBufferMem              = data->vdmRingBuffer;
    data->contextParams.vdmRingBufferMemSize          = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
    data->contextParams.vertexRingBufferMem           = data->vertexRingBuffer;
    data->contextParams.vertexRingBufferMemSize       = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
    data->contextParams.fragmentRingBufferMem         = data->fragmentRingBuffer;
    data->contextParams.fragmentRingBufferMemSize     = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
    data->contextParams.fragmentUsseRingBufferMem     = data->fragmentUsseRingBuffer;
    data->contextParams.fragmentUsseRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
    data->contextParams.fragmentUsseRingBufferOffset  = fragmentUsseRingBufferOffset;

    err = sceGxmCreateContext(&data->contextParams, &data->gxm_context);
    if (err != SCE_OK) {
        SDL_SetError("create context failed: %d\n", err);
        return err;
    }

    // set up parameters
    SceGxmRenderTargetParams renderTargetParams;
    SDL_memset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
    renderTargetParams.flags                = 0;
    renderTargetParams.width                = VITA_GXM_SCREEN_WIDTH;
    renderTargetParams.height               = VITA_GXM_SCREEN_HEIGHT;
    renderTargetParams.scenesPerFrame       = MAX_SCENES_PER_FRAME;
    renderTargetParams.multisampleMode      = 0;
    renderTargetParams.multisampleLocations = 0;
    renderTargetParams.driverMemBlock       = -1; // Invalid UID

    // create the render target
    err = sceGxmCreateRenderTarget(&renderTargetParams, &data->renderTarget);
    if (err != SCE_OK) {
        SDL_SetError("render target creation failed: %d\n", err);
        return err;
    }

    // allocate memory and sync objects for display buffers
    for (i = 0; i < VITA_GXM_BUFFERS; i++) {

        // allocate memory for display
        data->displayBufferData[i] = gpu_alloc_mapped_aligned(4096, 4 * VITA_GXM_SCREEN_STRIDE * VITA_GXM_SCREEN_HEIGHT, VITA_MEM_VRAM);

        // memset the buffer to black
        SDL_memset(data->displayBufferData[i], 0, VITA_GXM_SCREEN_HEIGHT * VITA_GXM_SCREEN_STRIDE * 4);

        // initialize a color surface for this display buffer
        err = sceGxmColorSurfaceInit(
            &data->displaySurface[i],
            VITA_GXM_COLOR_FORMAT,
            SCE_GXM_COLOR_SURFACE_LINEAR,
            SCE_GXM_COLOR_SURFACE_SCALE_NONE,
            SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
            VITA_GXM_SCREEN_WIDTH,
            VITA_GXM_SCREEN_HEIGHT,
            VITA_GXM_SCREEN_STRIDE,
            data->displayBufferData[i]
        );

        if (err != SCE_OK) {
            SDL_SetError("color surface init failed: %d\n", err);
            return err;
        }

        // create a sync object that we will associate with this buffer
        err = sceGxmSyncObjectCreate(&data->displayBufferSync[i]);
        if (err != SCE_OK) {
            SDL_SetError("sync object creation failed: %d\n", err);
            return err;
        }
    }

    // compute the memory footprint of the depth buffer
    const unsigned int alignedWidth = ALIGN(VITA_GXM_SCREEN_WIDTH, SCE_GXM_TILE_SIZEX);
    const unsigned int alignedHeight = ALIGN(VITA_GXM_SCREEN_HEIGHT, SCE_GXM_TILE_SIZEY);

    unsigned int sampleCount = alignedWidth * alignedHeight;
    unsigned int depthStrideInSamples = alignedWidth;

    // allocate the depth buffer
    data->depthBufferData = gpu_alloc_mapped_aligned(SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT, 4 * sampleCount, use_vram ? VITA_MEM_VRAM : VITA_MEM_RAM);

    // allocate the stencil buffer
    data->stencilBufferData = gpu_alloc_mapped_aligned(SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT, 4 * sampleCount, use_vram ? VITA_MEM_VRAM : VITA_MEM_RAM);

    // create the SceGxmDepthStencilSurface structure
    err = sceGxmDepthStencilSurfaceInit(
        &data->depthSurface,
        SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
        SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
        depthStrideInSamples,
        data->depthBufferData,
        data->stencilBufferData);

    // set the stencil test reference (this is currently assumed to always remain 1 after here for region clipping)
    sceGxmSetFrontStencilRef(data->gxm_context, 1);

    // set the stencil function (this wouldn't actually be needed, as the set clip rectangle function has to call this at the begginning of every scene)
    sceGxmSetFrontStencilFunc(
        data->gxm_context,
        SCE_GXM_STENCIL_FUNC_ALWAYS,
        SCE_GXM_STENCIL_OP_KEEP,
        SCE_GXM_STENCIL_OP_KEEP,
        SCE_GXM_STENCIL_OP_KEEP,
        0xFF,
        0xFF);

    // set buffer sizes for this sample
    const unsigned int patcherBufferSize        = 64*1024;
    const unsigned int patcherVertexUsseSize    = 64*1024;
    const unsigned int patcherFragmentUsseSize  = 64*1024;

    // allocate memory for buffers and USSE code
    data->patcherBuffer = gpu_alloc_mapped_aligned(4096, patcherBufferSize, use_vram ? VITA_MEM_VRAM : VITA_MEM_RAM);

    unsigned int patcherVertexUsseOffset;
    data->patcherVertexUsse = gpu_vertex_usse_alloc_mapped(patcherVertexUsseSize, &patcherVertexUsseOffset);

    unsigned int patcherFragmentUsseOffset;
    data->patcherFragmentUsse = gpu_fragment_usse_alloc_mapped(patcherFragmentUsseSize, &patcherFragmentUsseOffset);

    // create a shader patcher
    SceGxmShaderPatcherParams patcherParams;
    SDL_memset(&patcherParams, 0, sizeof(SceGxmShaderPatcherParams));
    patcherParams.userData                  = NULL;
    patcherParams.hostAllocCallback         = &patcher_host_alloc;
    patcherParams.hostFreeCallback          = &patcher_host_free;
    patcherParams.bufferAllocCallback       = NULL;
    patcherParams.bufferFreeCallback        = NULL;
    patcherParams.bufferMem                 = data->patcherBuffer;
    patcherParams.bufferMemSize             = patcherBufferSize;
    patcherParams.vertexUsseAllocCallback   = NULL;
    patcherParams.vertexUsseFreeCallback    = NULL;
    patcherParams.vertexUsseMem             = data->patcherVertexUsse;
    patcherParams.vertexUsseMemSize         = patcherVertexUsseSize;
    patcherParams.vertexUsseOffset          = patcherVertexUsseOffset;
    patcherParams.fragmentUsseAllocCallback = NULL;
    patcherParams.fragmentUsseFreeCallback  = NULL;
    patcherParams.fragmentUsseMem           = data->patcherFragmentUsse;
    patcherParams.fragmentUsseMemSize       = patcherFragmentUsseSize;
    patcherParams.fragmentUsseOffset        = patcherFragmentUsseOffset;

    err = sceGxmShaderPatcherCreate(&patcherParams, &data->shaderPatcher);
    if (err != SCE_OK) {
        SDL_SetError("shader patcher creation failed: %d\n", err);
        return err;
    }

    err = sceGxmProgramCheck(textureVertexProgramGxp);
    if (err != SCE_OK) {
        SDL_SetError("check program (texture vertex) failed: %d\n", err);
        return err;
    }

    err = sceGxmProgramCheck(textureFragmentProgramGxp);
    if (err != SCE_OK) {
        SDL_SetError("check program (texture fragment) failed: %d\n", err);
        return err;
    }

    err = sceGxmProgramCheck(clearVertexProgramGxp);
    if (err != SCE_OK) {
        SDL_SetError("check program (clear vertex) failed: %d\n", err);
        return err;
    }

    err = sceGxmProgramCheck(clearFragmentProgramGxp);
    if (err != SCE_OK) {
        SDL_SetError("check program (clear fragment) failed: %d\n", err);
        return err;
    }

    // register programs with the patcher
    err = sceGxmShaderPatcherRegisterProgram(data->shaderPatcher, textureVertexProgramGxp, &data->textureVertexProgramId);
    if (err != SCE_OK) {
        SDL_SetError("register program (texture vertex) failed: %d\n", err);
        return err;
    }

    err = sceGxmShaderPatcherRegisterProgram(data->shaderPatcher, textureFragmentProgramGxp, &data->textureFragmentProgramId);
    if (err != SCE_OK) {
        SDL_SetError("register program (texture fragment) failed: %d\n", err);
        return err;
    }

    err = sceGxmShaderPatcherRegisterProgram(data->shaderPatcher, clearVertexProgramGxp, &data->clearVertexProgramId);
    if (err != SCE_OK) {
        SDL_SetError("register program (clear vertex) failed: %d\n", err);
        return err;
    }

    err = sceGxmShaderPatcherRegisterProgram(data->shaderPatcher, clearFragmentProgramGxp, &data->clearFragmentProgramId);
    if (err != SCE_OK) {
        SDL_SetError("register program (clear fragment) failed: %d\n", err);
        return err;
    }

    {
        // get attributes by name to create vertex format bindings
        const SceGxmProgramParameter *paramClearPositionAttribute = sceGxmProgramFindParameterByName(clearVertexProgramGxp, "aPosition");

        // create clear vertex format
        SceGxmVertexAttribute clearVertexAttributes[1];
        SceGxmVertexStream clearVertexStreams[1];
        clearVertexAttributes[0].streamIndex    = 0;
        clearVertexAttributes[0].offset         = 0;
        clearVertexAttributes[0].format         = SCE_GXM_ATTRIBUTE_FORMAT_F32;
        clearVertexAttributes[0].componentCount = 2;
        clearVertexAttributes[0].regIndex       = sceGxmProgramParameterGetResourceIndex(paramClearPositionAttribute);
        clearVertexStreams[0].stride            = sizeof(clear_vertex);
        clearVertexStreams[0].indexSource       = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

        // create clear programs
        err = sceGxmShaderPatcherCreateVertexProgram(
            data->shaderPatcher,
            data->clearVertexProgramId,
            clearVertexAttributes,
            1,
            clearVertexStreams,
            1,
            &data->clearVertexProgram
        );
        if (err != SCE_OK) {
            SDL_SetError("create program (clear vertex) failed: %d\n", err);
            return err;
        }

        err = sceGxmShaderPatcherCreateFragmentProgram(
            data->shaderPatcher,
            data->clearFragmentProgramId,
            SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
            0,
            NULL,
            clearVertexProgramGxp,
            &data->clearFragmentProgram
        );
        if (err != SCE_OK) {
            SDL_SetError("create program (clear fragment) failed: %d\n", err);
            return err;
        }

        // create the clear triangle vertex/index data
        data->clearVertices = (clear_vertex *)gpu_alloc_mapped_aligned(4096, 3*sizeof(clear_vertex), use_vram ? VITA_MEM_VRAM : VITA_MEM_RAM);

        data->clearVertices[0].x = -1.0f;
        data->clearVertices[0].y = -1.0f;
        data->clearVertices[1].x =  3.0f;
        data->clearVertices[1].y = -1.0f;
        data->clearVertices[2].x = -1.0f;
        data->clearVertices[2].y =  3.0f;
    }

    // Allocate a 4 * 2 bytes = 8 bytes buffer and store all possible
    // 16-bit indices in linear ascending order, so we can use this for
    // all drawing operations where we don't want to use indexing.
    data->linearIndices = (uint16_t *)gpu_alloc_mapped_aligned(sizeof(uint16_t), 4*sizeof(uint16_t), use_vram ? VITA_MEM_VRAM : VITA_MEM_RAM);

    for (uint16_t i = 0; i < 4; ++i)
    {
        data->linearIndices[i] = i;
    }

    const SceGxmProgramParameter *paramTexturePositionAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aPosition");
    const SceGxmProgramParameter *paramTextureTexcoordAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aTexcoord");

    // create texture vertex format
    SceGxmVertexAttribute textureVertexAttributes[2];
    SceGxmVertexStream textureVertexStreams[1];
    /* x,y,z: 3 float 32 bits */
    textureVertexAttributes[0].streamIndex = 0;
    textureVertexAttributes[0].offset = 0;
    textureVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
    textureVertexAttributes[0].componentCount = 3; // (x, y, z)
    textureVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramTexturePositionAttribute);
    /* u,v: 2 floats 32 bits */
    textureVertexAttributes[1].streamIndex = 0;
    textureVertexAttributes[1].offset = 12; // (x, y, z) * 4 = 12 bytes
    textureVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
    textureVertexAttributes[1].componentCount = 2; // (u, v)
    textureVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramTextureTexcoordAttribute);
    // 16 bit (short) indices
    textureVertexStreams[0].stride = sizeof(texture_vertex);
    textureVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

    // create texture shaders
    err = sceGxmShaderPatcherCreateVertexProgram(
        data->shaderPatcher,
        data->textureVertexProgramId,
        textureVertexAttributes,
        2,
        textureVertexStreams,
        1,
        &data->textureVertexProgram
    );
    if (err != SCE_OK) {
        SDL_SetError("create program (texture vertex) failed: %d\n", err);
        return err;
    }

    // Fill SceGxmBlendInfo
    static const SceGxmBlendInfo blend_info = {
        .colorFunc = SCE_GXM_BLEND_FUNC_NONE,
        .alphaFunc = SCE_GXM_BLEND_FUNC_NONE,
        .colorSrc  = SCE_GXM_BLEND_FACTOR_ZERO,
        .colorDst  = SCE_GXM_BLEND_FACTOR_ZERO,
        .alphaSrc  = SCE_GXM_BLEND_FACTOR_ZERO,
        .alphaDst  = SCE_GXM_BLEND_FACTOR_ZERO,
        .colorMask = SCE_GXM_COLOR_MASK_ALL
    };

    err = sceGxmShaderPatcherCreateFragmentProgram(
        data->shaderPatcher,
        data->textureFragmentProgramId,
        SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
        0,
        &blend_info,
        textureVertexProgramGxp,
        &data->textureFragmentProgram
    );

    if (err != SCE_OK) {
        SDL_SetError("Patcher create fragment failed: %d\n", err);
        return err;
    }

    // find vertex uniforms by name and cache parameter information
    data->textureWvpParam = (SceGxmProgramParameter *)sceGxmProgramFindParameterByName(textureVertexProgramGxp, "wvp");
    data->clearClearColorParam = (SceGxmProgramParameter *)sceGxmProgramFindParameterByName(clearFragmentProgramGxp, "uClearColor");

    // Allocate memory for screen vertices
    data->screenVertices = gpu_alloc_mapped_aligned(sizeof(texture_vertex), 4*sizeof(texture_vertex), use_vram ? VITA_MEM_VRAM : VITA_MEM_RAM);

    init_orthographic_matrix(data->ortho_matrix, -1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f);

    notificationMem = sceGxmGetNotificationRegion();
    flipFragmentNotif.address = notificationMem;
    flipFragmentNotif.value = 1;
    *flipFragmentNotif.address = flipFragmentNotif.value;

#ifdef VITA_HW_ACCEL
    for (int i = 0; i < NOTIF_NUM; ++i)
    {
        notification_busy[i] = 0;
    }
#endif

    data->backBufferIndex = 0;
    data->frontBufferIndex = 0;

    sceGxmSetVertexProgram(data->gxm_context, data->textureVertexProgram);
    sceGxmSetFragmentProgram(data->gxm_context, data->textureFragmentProgram);

    return 0;
}

void gxm_finish()
{
    gxm_wait_rendering_done();

    // clean up allocations
    sceGxmShaderPatcherReleaseVertexProgram(data->shaderPatcher, data->textureVertexProgram);
    sceGxmShaderPatcherReleaseVertexProgram(data->shaderPatcher, data->clearVertexProgram);
    sceGxmShaderPatcherReleaseFragmentProgram(data->shaderPatcher, data->textureFragmentProgram);
    sceGxmShaderPatcherReleaseFragmentProgram(data->shaderPatcher, data->clearFragmentProgram);

    vgl_free(data->linearIndices);
    vgl_free(data->clearVertices);

    // wait until display queue is finished before deallocating display buffers
    sceGxmDisplayQueueFinish();

    for (size_t i = 0; i < VITA_GXM_BUFFERS; i++)
    {
        // clear the buffer then deallocate
        SDL_memset(data->displayBufferData[i], 0, VITA_GXM_SCREEN_HEIGHT * VITA_GXM_SCREEN_STRIDE * 4);
        vgl_free(data->displayBufferData[i]);

        // destroy the sync object
        sceGxmSyncObjectDestroy(data->displayBufferSync[i]);
    }

    // free the depth and stencil buffer
    vgl_free(data->depthBufferData);
    vgl_free(data->stencilBufferData);

    // unregister programs and destroy shader patcher
    sceGxmShaderPatcherUnregisterProgram(data->shaderPatcher, data->textureFragmentProgramId);
    sceGxmShaderPatcherUnregisterProgram(data->shaderPatcher, data->textureVertexProgramId);
    sceGxmShaderPatcherUnregisterProgram(data->shaderPatcher, data->clearFragmentProgramId);
    sceGxmShaderPatcherUnregisterProgram(data->shaderPatcher, data->clearVertexProgramId);

    sceGxmShaderPatcherDestroy(data->shaderPatcher);
    gpu_fragment_usse_free_mapped(data->patcherFragmentUsse);
    gpu_vertex_usse_free_mapped(data->patcherVertexUsse);
    vgl_free(data->patcherBuffer);

    // destroy the render target
    sceGxmDestroyRenderTarget(data->renderTarget);

    // destroy the gxm context
    sceGxmDestroyContext(data->gxm_context);
    gpu_fragment_usse_free_mapped(data->fragmentUsseRingBuffer);
    vgl_free(data->fragmentRingBuffer);
    vgl_free(data->vertexRingBuffer);
    vgl_free(data->vdmRingBuffer);
    SDL_free(data->contextParams.hostMem);
    vgl_free(data->screenVertices);

    vgl_mem_term();

    // terminate libgxm
    sceGxmTerminate();

    SDL_free(data);
}

void free_gxm_texture(gxm_texture *texture)
{
    if (texture) {
        if (texture->palette) {
            vgl_free(texture->palette);
        }
        vgl_free(texture->data);
#ifdef VITA_HW_ACCEL
        notification_busy[texture->notification_id] = 0;
#endif
        SDL_free(texture);
    }
}

SceGxmTextureFormat gxm_texture_get_format(const gxm_texture *texture)
{
    return sceGxmTextureGetFormat(&texture->gxm_tex);
}

unsigned int gxm_texture_get_width(const gxm_texture *texture)
{
    return sceGxmTextureGetWidth(&texture->gxm_tex);
}

unsigned int gxm_texture_get_height(const gxm_texture *texture)
{
    return sceGxmTextureGetHeight(&texture->gxm_tex);
}

unsigned int gxm_texture_get_stride(const gxm_texture *texture)
{
    return ((gxm_texture_get_width(texture) + 7) & ~7)
        * tex_format_to_bytespp(gxm_texture_get_format(texture));
}

void *gxm_texture_get_datap(const gxm_texture *texture)
{
    return sceGxmTextureGetData(&texture->gxm_tex);
}

void *gxm_texture_get_palette(const gxm_texture *texture)
{
    return sceGxmTextureGetPalette(&texture->gxm_tex);
}

void gxm_texture_set_alloc_memblock_type(VitaMemType type)
{
    textureMemBlockType = type;
}

gxm_texture* create_gxm_texture(unsigned int w, unsigned int h, SceGxmTextureFormat format)
{
    gxm_texture *texture = SDL_calloc(sizeof(gxm_texture), 1);
    if (!texture)
        return NULL;

    const int tex_size =  ((w + 7) & ~ 7) * h * tex_format_to_bytespp(format);

    /* Allocate a GPU buffer for the texture */
    texture->data = gpu_alloc_mapped_aligned(SCE_GXM_TEXTURE_ALIGNMENT, tex_size, textureMemBlockType);

    if (!texture->data) {
        free(texture);
        return NULL;
    }

    /* Clear the texture */
    SDL_memset(texture->data, 0, tex_size);

    /* Create the gxm texture */
    sceGxmTextureInitLinear(&texture->gxm_tex, texture->data, format, w, h, 0);

    if ((format & 0x9f000000U) == SCE_GXM_TEXTURE_BASE_FORMAT_P8) {
        const int pal_size = 256 * sizeof(uint32_t);

        texture->palette = gpu_alloc_mapped_aligned(SCE_GXM_PALETTE_ALIGNMENT, pal_size, VITA_MEM_VRAM);

        if (!texture->palette) {
            free_gxm_texture(texture);
            return NULL;
        }

        SDL_memset(texture->palette, 0, pal_size);

        sceGxmTextureSetPalette(&texture->gxm_tex, texture->palette);
    } else {
        texture->palette = NULL;
    }

#ifdef VITA_HW_ACCEL
    int free_notification_found = 0;
    // notification 0 is reserved for screen flip
    for (int i = 1; i < NOTIF_NUM; ++i)
    {
        if (!notification_busy[i])
        {
            notification_busy[i] = 1;
            texture->notification_id = i;
            texture->fragment_notif.address = notificationMem + i;
            texture->fragment_notif.value = 1;
            *texture->fragment_notif.address = texture->fragment_notif.value;
            free_notification_found = 1;
            break;
        }
    }

    if (!free_notification_found)
    {
        //just use notification 512 in this case and hope for the best
        texture->notification_id = NOTIF_NUM - 1;
        texture->fragment_notif.address = notificationMem + NOTIF_NUM - 1;
        notification_limit_reached = 1;
    }
#endif

    return texture;
}

void gxm_init_texture_scale(const gxm_texture *texture, float x, float y, float x_scale, float y_scale)
{
    const int tex_w = gxm_texture_get_width(texture);
    const int tex_h = gxm_texture_get_height(texture);
    const float w = x_scale * tex_w;
    const float h = y_scale * tex_h;

    data->screenVertices[0].x = (x / VITA_GXM_SCREEN_WIDTH * 2.0f) - 1.0f;
    data->screenVertices[0].y = (y / VITA_GXM_SCREEN_HEIGHT * 2.0f - 1.0f) * -1.0f;
    data->screenVertices[0].z = +0.5f;
    data->screenVertices[0].u = 0.0f;
    data->screenVertices[0].v = 0.0f;

    data->screenVertices[1].x = (x + w) / VITA_GXM_SCREEN_WIDTH * 2.0f - 1.0f;
    data->screenVertices[1].y = (y / VITA_GXM_SCREEN_HEIGHT * 2.0f - 1.0f) * -1.0f;
    data->screenVertices[1].z = +0.5f;
    data->screenVertices[1].u = 1.0f;
    data->screenVertices[1].v = 0.0f;

    data->screenVertices[2].x = (x / VITA_GXM_SCREEN_WIDTH * 2.0f) - 1.0f;
    data->screenVertices[2].y = ((y + h) / VITA_GXM_SCREEN_HEIGHT * 2.0f - 1.0f) * -1.0f;
    data->screenVertices[2].z = +0.5f;
    data->screenVertices[2].u = 0.0f;
    data->screenVertices[2].v = 1.0f;

    data->screenVertices[3].x = (x + w) / VITA_GXM_SCREEN_WIDTH * 2.0f - 1.0f;
    data->screenVertices[3].y = ((y + h) / VITA_GXM_SCREEN_HEIGHT * 2.0f - 1.0f) * -1.0f;
    data->screenVertices[3].z = +0.5f;
    data->screenVertices[3].u = 1.0f;
    data->screenVertices[3].v = 1.0f;

    // Set the texture to the TEXUNIT0
    sceGxmSetFragmentTexture(data->gxm_context, 0, &texture->gxm_tex);
    sceGxmSetVertexStream(data->gxm_context, 0, data->screenVertices);
}

void gxm_wait_rendering_done()
{
#ifdef VITA_HW_ACCEL
    sceGxmTransferFinish();
#endif
    sceGxmFinish(data->gxm_context);
}

void gxm_texture_set_filters(gxm_texture *texture, SceGxmTextureFilter min_filter, SceGxmTextureFilter mag_filter)
{
    sceGxmTextureSetMinFilter(&texture->gxm_tex, min_filter);
    sceGxmTextureSetMagFilter(&texture->gxm_tex, mag_filter);
}

void gxm_set_vblank_wait(int enable)
{
    data->displayData.vblank_wait = enable;
}

void gxm_render_clear()
{
    void *color_buffer;
    float clear_color[4];

    clear_color[0] = 0;
    clear_color[1] = 0;
    clear_color[2] = 0;
    clear_color[3] = 1;

    // set clear shaders
    sceGxmSetVertexProgram(data->gxm_context, data->clearVertexProgram);
    sceGxmSetFragmentProgram(data->gxm_context, data->clearFragmentProgram);

    // set the clear color
    sceGxmReserveFragmentDefaultUniformBuffer(data->gxm_context, &color_buffer);
    sceGxmSetUniformDataF(color_buffer, data->clearClearColorParam, 0, 4, clear_color);

    // draw the clear triangle
    sceGxmSetVertexStream(data->gxm_context, 0, data->clearVertices);
    sceGxmDraw(data->gxm_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, data->linearIndices, 3);

    // set back the texture program
    sceGxmSetVertexProgram(data->gxm_context, data->textureVertexProgram);
    sceGxmSetFragmentProgram(data->gxm_context, data->textureFragmentProgram);
    sceGxmSetVertexStream(data->gxm_context, 0, data->screenVertices);
}

void gxm_draw_screen_texture(gxm_texture *texture, int clear_required)
{
    //SCE_GXM_SCENE_FRAGMENT_TRANSFER_SYNC
    sceGxmBeginScene(
        data->gxm_context,
        0,
        data->renderTarget,
        NULL,
        NULL,
        data->displayBufferSync[data->backBufferIndex],
        &data->displaySurface[data->backBufferIndex],
        &data->depthSurface
    );

    if (clear_required)
    {
        gxm_render_clear();
    }

    void *vertex_wvp_buffer;
    sceGxmReserveVertexDefaultUniformBuffer(data->gxm_context, &vertex_wvp_buffer);
    sceGxmSetUniformDataF(vertex_wvp_buffer, data->textureWvpParam, 0, 16, data->ortho_matrix);
    sceGxmDraw(data->gxm_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, data->linearIndices, 4);
#ifdef VITA_HW_ACCEL
    // make sure that transfers are finished before rendering the texture
    if (!notification_limit_reached) {
        sceGxmNotificationWait(&texture->fragment_notif);
    } else {
        sceGxmTransferFinish();
    }
#endif
    *flipFragmentNotif.address = 0;
    sceGxmEndScene(data->gxm_context, NULL, &flipFragmentNotif);
    lastScreenTexture = texture;

    data->displayData.address = data->displayBufferData[data->backBufferIndex];

    SceCommonDialogUpdateParam updateParam;
    SDL_memset(&updateParam, 0, sizeof(updateParam));

    updateParam.renderTarget.colorFormat    = VITA_GXM_COLOR_FORMAT;
    updateParam.renderTarget.surfaceType    = SCE_GXM_COLOR_SURFACE_LINEAR;
    updateParam.renderTarget.width          = VITA_GXM_SCREEN_WIDTH;
    updateParam.renderTarget.height         = VITA_GXM_SCREEN_HEIGHT;
    updateParam.renderTarget.strideInPixels = VITA_GXM_SCREEN_STRIDE;

    updateParam.renderTarget.colorSurfaceData = data->displayBufferData[data->backBufferIndex];
    updateParam.renderTarget.depthSurfaceData = data->depthBufferData;

    updateParam.displaySyncObject = (SceGxmSyncObject *)data->displayBufferSync[data->backBufferIndex];

    sceCommonDialogUpdate(&updateParam);

    sceGxmDisplayQueueAddEntry(
        data->displayBufferSync[data->frontBufferIndex],    // OLD fb
        data->displayBufferSync[data->backBufferIndex],     // NEW fb
        &data->displayData
    );

    // update buffer indices
    data->frontBufferIndex = data->backBufferIndex;
    data->backBufferIndex = (data->backBufferIndex + 1) % VITA_GXM_BUFFERS;
}

#ifdef VITA_HW_ACCEL
// what if locked texture is currently being read from..? probably still safer to use gxm_wait_rendering_done() for locking
// also there's a possibility of more than 1 queued job that may result in fired up notification while there are still jobs left to do
// rework of notification system or ensuring that jobs are finished before queuing new ones is probably a safer approach (and slower one)
void gxm_lock_texture(gxm_texture *texture)
{
    if (!notification_limit_reached) {
        sceGxmNotificationWait(&texture->fragment_notif);
    } else {
        sceGxmTransferFinish();
    }
}

SceGxmTransferFormat gxm_texture_get_transferformat(const gxm_texture *texture)
{
    SceGxmTextureFormat texFormat = gxm_texture_get_format(texture);
    SceGxmTransferFormat transferFormat;

    switch (texFormat)
    {
        case SCE_GXM_TEXTURE_FORMAT_P8_1BGR:
        case SCE_GXM_TEXTURE_FORMAT_P8_1RGB:
        case SCE_GXM_TEXTURE_FORMAT_P8_ABGR:
        case SCE_GXM_TEXTURE_FORMAT_P8_ARGB:
            transferFormat = SCE_GXM_TRANSFER_FORMAT_U8_R;
            break;
        case SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ABGR:
        case SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ARGB:
            transferFormat = SCE_GXM_TRANSFER_FORMAT_U1U5U5U5_ABGR;
            break;
        case SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR:
        case SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB:
            transferFormat = SCE_GXM_TRANSFER_FORMAT_U5U6U5_BGR;
            break;
        case SCE_GXM_TEXTURE_FORMAT_U8U8U8_BGR:
        case SCE_GXM_TEXTURE_FORMAT_U8U8U8_RGB:
            transferFormat = SCE_GXM_TRANSFER_FORMAT_U8U8U8_BGR;
            break;
        case SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR:
        case SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB:
            transferFormat = SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR;
            break;
        default:
            SDL_SetError("Invalid texture format!\n");
            break;
    }
    return transferFormat;
}

void gxm_fill_rect_transfer(gxm_texture *dst, SDL_Rect dstrect, uint32_t color)
{
    // make sure that screen rendering is finished before writing on the same texture
    if (dst == lastScreenTexture)
    {
        lastScreenTexture = NULL;
        sceGxmNotificationWait(&flipFragmentNotif);
    }

    SceGxmTransferFormat transferFormat = gxm_texture_get_transferformat(dst);

    *dst->fragment_notif.address = 0;

    // sceGxmNotificationWait could lock in theory on failed sceGxmTransfer or sceGxmEndScene
    sceGxmTransferFill(color, 
        transferFormat,
        gxm_texture_get_datap(dst),
        dstrect.x,
        dstrect.y,
        dstrect.w,
        dstrect.h,
        gxm_texture_get_stride(dst),
        NULL,
        0,
        &dst->fragment_notif
    );
}

void gxm_blit_transfer(gxm_texture *src, SDL_Rect srcrect, gxm_texture *dst, SDL_Rect dstrect, int colorkey_enabled, Uint32 colorkey, Uint32 colorkeyMask)
{
    // make sure that screen rendering is finished before writing on the same texture
    if (dst == lastScreenTexture)
    {
        lastScreenTexture = NULL;
        sceGxmNotificationWait(&flipFragmentNotif);
    }

    SceGxmTransferFormat srcTransferFormat = gxm_texture_get_transferformat(src);
    SceGxmTransferFormat dstTransferFormat = gxm_texture_get_transferformat(dst);

    *dst->fragment_notif.address = 0;

    sceGxmTransferCopy(
        srcrect.w,
        srcrect.h,
        colorkey,
        colorkeyMask,
        colorkey_enabled ? SCE_GXM_TRANSFER_COLORKEY_REJECT : SCE_GXM_TRANSFER_COLORKEY_NONE,
        srcTransferFormat,
        SCE_GXM_TRANSFER_LINEAR,
        gxm_texture_get_datap(src),
        srcrect.x,
        srcrect.y,
        gxm_texture_get_stride(src),
        dstTransferFormat,
        SCE_GXM_TRANSFER_LINEAR,
        gxm_texture_get_datap(dst),
        dstrect.x,
        dstrect.y,
        gxm_texture_get_stride(dst),
        NULL,
        0,
        &dst->fragment_notif
    );
}
#endif

static unsigned int back_buffer_index_for_common_dialog = 0;
static unsigned int front_buffer_index_for_common_dialog = 0;

struct
{
    VITA_GXM_DisplayData displayData;
    SceGxmSyncObject* sync;
    SceGxmColorSurface surf;
} buffer_for_common_dialog[VITA_GXM_BUFFERS];

void gxm_minimal_init_for_common_dialog(void)
{
    SceGxmInitializeParams initializeParams;
    SDL_memset(&initializeParams, 0, sizeof(initializeParams));
    initializeParams.flags                          = 0;
    initializeParams.displayQueueMaxPendingCount    = VITA_GXM_PENDING_SWAPS;
    initializeParams.displayQueueCallback           = display_callback;
    initializeParams.displayQueueCallbackDataSize   = sizeof(VITA_GXM_DisplayData);
    initializeParams.parameterBufferSize            = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;
    sceGxmInitialize(&initializeParams);
    vgl_mem_init(0, 32 * 1024 * 1024, 0);
}

void gxm_minimal_term_for_common_dialog(void)
{
    sceGxmTerminate();
    vgl_mem_term();
}

void gxm_init_for_common_dialog(void)
{
    for (int i = 0; i < VITA_GXM_BUFFERS; i += 1)
    {
        buffer_for_common_dialog[i].displayData.vblank_wait = SDL_TRUE;
        buffer_for_common_dialog[i].displayData.address = gpu_alloc_mapped_aligned(
            4096,
            4 * VITA_GXM_SCREEN_STRIDE * VITA_GXM_SCREEN_HEIGHT,
            use_vram ? VITA_MEM_VRAM : VITA_MEM_RAM);

        sceGxmColorSurfaceInit(
            &buffer_for_common_dialog[i].surf,
            VITA_GXM_PIXEL_FORMAT,
            SCE_GXM_COLOR_SURFACE_LINEAR,
            SCE_GXM_COLOR_SURFACE_SCALE_NONE,
            SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
            VITA_GXM_SCREEN_WIDTH,
            VITA_GXM_SCREEN_HEIGHT,
            VITA_GXM_SCREEN_STRIDE,
            buffer_for_common_dialog[i].displayData.address
        );
        sceGxmSyncObjectCreate(&buffer_for_common_dialog[i].sync);
    }
    sceGxmDisplayQueueFinish();
}

void gxm_swap_for_common_dialog(void)
{
    SceCommonDialogUpdateParam updateParam;
    SDL_memset(&updateParam, 0, sizeof(updateParam));

    updateParam.renderTarget.colorFormat    = VITA_GXM_PIXEL_FORMAT;
    updateParam.renderTarget.surfaceType    = SCE_GXM_COLOR_SURFACE_LINEAR;
    updateParam.renderTarget.width          = VITA_GXM_SCREEN_WIDTH;
    updateParam.renderTarget.height         = VITA_GXM_SCREEN_HEIGHT;
    updateParam.renderTarget.strideInPixels = VITA_GXM_SCREEN_STRIDE;

    updateParam.renderTarget.colorSurfaceData = buffer_for_common_dialog[back_buffer_index_for_common_dialog].displayData.address;

    updateParam.displaySyncObject = buffer_for_common_dialog[back_buffer_index_for_common_dialog].sync;
    SDL_memset(buffer_for_common_dialog[back_buffer_index_for_common_dialog].displayData.address, 0, 4 * VITA_GXM_SCREEN_STRIDE * VITA_GXM_SCREEN_HEIGHT);
    sceCommonDialogUpdate(&updateParam);

    sceGxmDisplayQueueAddEntry(buffer_for_common_dialog[front_buffer_index_for_common_dialog].sync, buffer_for_common_dialog[back_buffer_index_for_common_dialog].sync, &buffer_for_common_dialog[back_buffer_index_for_common_dialog].displayData);
    front_buffer_index_for_common_dialog = back_buffer_index_for_common_dialog;
    back_buffer_index_for_common_dialog = (back_buffer_index_for_common_dialog + 1) % VITA_GXM_BUFFERS;
}

void gxm_term_for_common_dialog(void)
{
    sceGxmDisplayQueueFinish();
    for (int i = 0; i < VITA_GXM_BUFFERS; i += 1)
    {
        vgl_free(buffer_for_common_dialog[i].displayData.address);
        sceGxmSyncObjectDestroy(buffer_for_common_dialog[i].sync);
    }
}
