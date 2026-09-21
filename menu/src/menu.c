/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include <string.h>
#include <switch.h>
#include <deko3d.h>

#define FB_NUM 2
#define FB_WIDTH 1280
#define FB_HEIGHT 720
#define CODEMEMSIZE (64 * 1024)
#define CMDMEMSIZE (16 * 1024)

extern const uint8_t triangle_vsh_dksh[];
extern const uint32_t triangle_vsh_dksh_size;
extern const uint8_t color_fsh_dksh[];
extern const uint32_t color_fsh_dksh_size;

static DkDevice s_device;
static DkMemBlock s_fbMem;
static DkImage s_fbs[FB_NUM];
static DkSwapchain s_swapchain;
static DkMemBlock s_codeMem;
static uint32_t s_codeOff;
static DkShader s_vsh;
static DkShader s_fsh;
static DkMemBlock s_cmdMem;
static DkCmdBuf s_cmdbuf;
static DkCmdList s_bindFb[FB_NUM];
static DkCmdList s_render;
static DkQueue s_queue;
static bool s_live;

static void load_embedded(DkShader *sh, const uint8_t *blob, uint32_t size)
{
    uint32_t off = s_codeOff;
    s_codeOff += (size + DK_SHADER_CODE_ALIGNMENT - 1) & ~(DK_SHADER_CODE_ALIGNMENT - 1);
    memcpy((uint8_t *)dkMemBlockGetCpuAddr(s_codeMem) + off, blob, size);
    DkShaderMaker mk;
    dkShaderMakerDefaults(&mk, s_codeMem, off);
    dkShaderInitialize(sh, &mk);
}

int onBoot(NWindow *win)
{
    if (s_live)
        return 0;
    DkDeviceMaker devMk;
    dkDeviceMakerDefaults(&devMk);
    s_device = dkDeviceCreate(&devMk);
    if (!s_device)
        return -1;

    DkImageLayoutMaker layMk;
    dkImageLayoutMakerDefaults(&layMk, s_device);
    layMk.flags = DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression;
    layMk.format = DkImageFormat_RGBA8_Unorm;
    layMk.dimensions[0] = FB_WIDTH;
    layMk.dimensions[1] = FB_HEIGHT;
    DkImageLayout lay;
    dkImageLayoutInitialize(&lay, &layMk);
    uint32_t fbSize = dkImageLayoutGetSize(&lay);
    uint32_t fbAlign = dkImageLayoutGetAlignment(&lay);
    fbSize = (fbSize + fbAlign - 1) & ~(fbAlign - 1);

    DkMemBlockMaker memMk;
    dkMemBlockMakerDefaults(&memMk, s_device, (uint32_t)(FB_NUM * fbSize));
    memMk.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    s_fbMem = dkMemBlockCreate(&memMk);
    if (!s_fbMem)
        return -2;

    DkImage const *imgs[FB_NUM];
    for (unsigned i = 0; i < FB_NUM; i++) {
        imgs[i] = &s_fbs[i];
        dkImageInitialize(&s_fbs[i], &lay, s_fbMem, i * fbSize);
    }

    DkSwapchainMaker scMk;
    dkSwapchainMakerDefaults(&scMk, s_device, win, imgs, FB_NUM);
    s_swapchain = dkSwapchainCreate(&scMk);
    if (!s_swapchain)
        return -3;

    dkMemBlockMakerDefaults(&memMk, s_device, CODEMEMSIZE);
    memMk.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code;
    s_codeMem = dkMemBlockCreate(&memMk);
    if (!s_codeMem)
        return -4;
    s_codeOff = 0;
    load_embedded(&s_vsh, triangle_vsh_dksh, triangle_vsh_dksh_size);
    load_embedded(&s_fsh, color_fsh_dksh, color_fsh_dksh_size);

    dkMemBlockMakerDefaults(&memMk, s_device, CMDMEMSIZE);
    memMk.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    s_cmdMem = dkMemBlockCreate(&memMk);
    if (!s_cmdMem)
        return -5;

    DkCmdBufMaker cbMk;
    dkCmdBufMakerDefaults(&cbMk, s_device);
    s_cmdbuf = dkCmdBufCreate(&cbMk);
    if (!s_cmdbuf)
        return -6;
    dkCmdBufAddMemory(s_cmdbuf, s_cmdMem, 0, CMDMEMSIZE);

    for (unsigned i = 0; i < FB_NUM; i++) {
        DkImageView view;
        dkImageViewDefaults(&view, &s_fbs[i]);
        dkCmdBufBindRenderTarget(s_cmdbuf, &view, NULL);
        s_bindFb[i] = dkCmdBufFinishList(s_cmdbuf);
    }

    DkViewport vp = { 0.0f, 0.0f, (float)FB_WIDTH, (float)FB_HEIGHT, 0.0f, 1.0f };
    DkScissor sc = { 0, 0, FB_WIDTH, FB_HEIGHT };
    DkShader const *shaders[] = { &s_vsh, &s_fsh };
    DkRasterizerState rast;
    DkColorState col;
    DkColorWriteState colW;
    dkRasterizerStateDefaults(&rast);
    dkColorStateDefaults(&col);
    dkColorWriteStateDefaults(&colW);
    dkCmdBufSetViewports(s_cmdbuf, 0, &vp, 1);
    dkCmdBufSetScissors(s_cmdbuf, 0, &sc, 1);
    dkCmdBufClearColorFloat(s_cmdbuf, 0, DkColorMask_RGBA, 0.125f, 0.294f, 0.478f, 1.0f);
    dkCmdBufBindShaders(s_cmdbuf, DkStageFlag_GraphicsMask, shaders, 2);
    dkCmdBufBindRasterizerState(s_cmdbuf, &rast);
    dkCmdBufBindColorState(s_cmdbuf, &col);
    dkCmdBufBindColorWriteState(s_cmdbuf, &colW);
    dkCmdBufDraw(s_cmdbuf, DkPrimitive_Triangles, 3, 1, 0, 0);
    s_render = dkCmdBufFinishList(s_cmdbuf);

    DkQueueMaker qMk;
    dkQueueMakerDefaults(&qMk, s_device);
    qMk.flags = DkQueueFlags_Graphics;
    s_queue = dkQueueCreate(&qMk);
    if (!s_queue)
        return -7;

    s_live = true;
    return 0;
}

int loop(void)
{
    if (!s_live)
        return -1;
    int slot = dkQueueAcquireImage(s_queue, s_swapchain);
    if (slot < 0)
        return -1;
    dkQueueSubmitCommands(s_queue, s_bindFb[slot]);
    dkQueueSubmitCommands(s_queue, s_render);
    dkQueuePresentImage(s_queue, s_swapchain, slot);
    return 0;
}

void onShutdown(void)
{
    if (!s_live)
        return;
    s_live = false;
    dkQueueWaitIdle(s_queue);
    dkQueueDestroy(s_queue);
    dkCmdBufDestroy(s_cmdbuf);
    dkMemBlockDestroy(s_cmdMem);
    dkMemBlockDestroy(s_codeMem);
    dkSwapchainDestroy(s_swapchain);
    dkMemBlockDestroy(s_fbMem);
    dkDeviceDestroy(s_device);
}

void onReload(void)
{
}

void onPowerButton(void)
{
}

void onOpen(void)
{
}

void onHomeButton(void)
{
}
