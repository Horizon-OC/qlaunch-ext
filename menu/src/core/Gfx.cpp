/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Gfx.hpp"
#include "Log.hpp"
#include <string.h>

extern const uint8_t quad_vsh_dksh[];
extern const uint32_t quad_vsh_dksh_size;
extern const uint8_t color_fsh_dksh[];
extern const uint32_t color_fsh_dksh_size;
extern const uint8_t text_vsh_dksh[];
extern const uint32_t text_vsh_dksh_size;
extern const uint8_t text_fsh_dksh[];
extern const uint32_t text_fsh_dksh_size;
extern const uint8_t panel_vsh_dksh[];
extern const uint32_t panel_vsh_dksh_size;
extern const uint8_t panel_fsh_dksh[];
extern const uint32_t panel_fsh_dksh_size;
extern const uint8_t icon_vsh_dksh[];
extern const uint32_t icon_vsh_dksh_size;
extern const uint8_t icon_fsh_dksh[];
extern const uint32_t icon_fsh_dksh_size;
extern const uint8_t bg_fsh_dksh[];
extern const uint32_t bg_fsh_dksh_size;
extern const uint8_t hl_vsh_dksh[];
extern const uint32_t hl_vsh_dksh_size;
extern const uint8_t hl_fsh_dksh[];
extern const uint32_t hl_fsh_dksh_size;

namespace Gfx {
static DkDevice device_;
static DkMemBlock fbMem_;
static DkImage fbs_[GFX_FB_NUM];
static DkSwapchain swapchain_;
static DkMemBlock codeMem_;
static unsigned codeOff_;
static DkShader vsh_, fsh_;
static DkShader tvsh_, tfsh_;
static DkShader pvsh_, pfsh_;
static DkShader ivsh_, ifsh_;
static DkShader bfsh_;
static DkShader hvsh_, hfsh_;
static DkMemBlock cmdMem_;
static DkCmdBuf cmdbuf_;
static DkQueue queue_;
static DkMemBlock vtxMem_;
static HomeVtx *vtxCpu_;
static DkGpuAddr vtxGpu_;
static unsigned vtxCount_;
static TextVtx *txtCpu_[3];
static DkGpuAddr txtGpu_[3];
static unsigned txtCount_[3];
static DkMemBlock pnlMem_;
static PanelVtx *pnlCpu_;
static DkGpuAddr pnlGpu_;
static unsigned pnlCount_;
static DkMemBlock icoMem_;
static IconVtx *icoCpu_;
static DkGpuAddr icoGpu_;
static unsigned icoCount_;
static DkMemBlock hlMem_;
static HighlightVtx *hlCpu_;
static DkGpuAddr hlGpu_;
static unsigned hlCount_;
static bool texUsed_[GFX_MAX_TEX];
static DkMemBlock descMem_;
static DkGpuAddr descGpu_;
static DkMemBlock uboMem_;
static DkGpuAddr uboGpu_;
static int fbW_, fbH_;
static float s_, ox_, oy_;
static int slot_;
static bool live_;
} /* namespace Gfx */

static void LoadOne(DkMemBlock codeMem, unsigned *off, DkShader *sh,
                    const uint8_t *blob, uint32_t size)
{
    uint32_t o = *off;
    *off += (size + DK_SHADER_CODE_ALIGNMENT - 1) & ~(DK_SHADER_CODE_ALIGNMENT - 1);
    memcpy((uint8_t *)dkMemBlockGetCpuAddr(codeMem) + o, blob, size);
    DkShaderMaker mk;
    dkShaderMakerDefaults(&mk, codeMem, o);
    dkShaderInitialize(sh, &mk);
}

static void FitTransform()
{
    Gfx::s_ = Gfx::fbW_ / 1920.0f;
    float sy = Gfx::fbH_ / 1080.0f;
    if (sy < Gfx::s_) Gfx::s_ = sy;
    Gfx::ox_ = (Gfx::fbW_ - 1920.0f * Gfx::s_) * 0.5f;
    Gfx::oy_ = (Gfx::fbH_ - 1080.0f * Gfx::s_) * 0.5f;
}

bool Gfx::Init(NWindow *win, int fbW, int fbH)
{
    fbW_ = fbW;
    fbH_ = fbH;
    FitTransform();

    DkDeviceMaker devMk;
    dkDeviceMakerDefaults(&devMk);
    device_ = dkDeviceCreate(&devMk);
    if (!device_)
        return false;

    DkImageLayout lay;
    uint32_t fbSize = 0;
    DkMemBlockMaker memMk;
    for (int attempt = 0; attempt < 2; attempt++) {
        DkImageLayoutMaker fbMk;
        dkImageLayoutMakerDefaults(&fbMk, device_);
        fbMk.flags = DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression;
        fbMk.format = DkImageFormat_RGBA8_Unorm;
        fbMk.dimensions[0] = (uint32_t)fbW_;
        fbMk.dimensions[1] = (uint32_t)fbH_;
        dkImageLayoutInitialize(&lay, &fbMk);
        fbSize = dkImageLayoutGetSize(&lay);
        uint32_t fbAlign = dkImageLayoutGetAlignment(&lay);
        fbSize = (fbSize + fbAlign - 1) & ~(fbAlign - 1);
        dkMemBlockMakerDefaults(&memMk, device_, (uint32_t)(GFX_FB_NUM * fbSize));
        memMk.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
        fbMem_ = dkMemBlockCreate(&memMk);
        if (fbMem_) break;
        fbW_ = 1280; fbH_ = 720; FitTransform();
    }
    if (!fbMem_) return false;

    DkImage const *imgs[GFX_FB_NUM];
    for (unsigned i = 0; i < GFX_FB_NUM; i++) {
        imgs[i] = &fbs_[i];
        dkImageInitialize(&fbs_[i], &lay, fbMem_, i * fbSize);
    }

    DkSwapchainMaker scMk;
    dkSwapchainMakerDefaults(&scMk, device_, win, imgs, GFX_FB_NUM);
    swapchain_ = dkSwapchainCreate(&scMk);
    if (!swapchain_)
        return false;

    dkMemBlockMakerDefaults(&memMk, device_, GFX_CODE_SIZE);
    memMk.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code;
    codeMem_ = dkMemBlockCreate(&memMk);
    if (!codeMem_)
        return false;
    if (!LoadShaders())
        return false;

    dkMemBlockMakerDefaults(&memMk, device_, GFX_CMD_SIZE);
    memMk.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    cmdMem_ = dkMemBlockCreate(&memMk);
    if (!cmdMem_)
        return false;

    uint32_t colorBytes = (uint32_t)(GFX_VTX_MAX * sizeof(HomeVtx));
    uint32_t textBytes = 3 * (uint32_t)(GFX_TEXT_MAX * sizeof(TextVtx));
    dkMemBlockMakerDefaults(&memMk, device_, colorBytes + textBytes);
    memMk.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    vtxMem_ = dkMemBlockCreate(&memMk);
    if (!vtxMem_)
        return false;
    vtxCpu_ = (HomeVtx *)dkMemBlockGetCpuAddr(vtxMem_);
    vtxGpu_ = dkMemBlockGetGpuAddr(vtxMem_);
    if (!vtxCpu_ || vtxGpu_ == DK_GPU_ADDR_INVALID)
        return false;
    uint32_t oneText = (uint32_t)(GFX_TEXT_MAX * sizeof(TextVtx));
    for (int i = 0; i < 3; i++) { txtCpu_[i] = (TextVtx *)((uint8_t *)vtxCpu_ + colorBytes + oneText * (uint32_t)i); txtGpu_[i] = vtxGpu_ + colorBytes + oneText * (uint32_t)i; txtCount_[i] = 0; }

    dkMemBlockMakerDefaults(&memMk, device_,
                            (uint32_t)(GFX_PANEL_MAX * sizeof(PanelVtx)));
    memMk.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    pnlMem_ = dkMemBlockCreate(&memMk);
    if (!pnlMem_)
        return false;
    pnlCpu_ = (PanelVtx *)dkMemBlockGetCpuAddr(pnlMem_);
    pnlGpu_ = dkMemBlockGetGpuAddr(pnlMem_);
    if (!pnlCpu_ || pnlGpu_ == DK_GPU_ADDR_INVALID)
        return false;

    dkMemBlockMakerDefaults(&memMk, device_,
                            (uint32_t)(GFX_ICONQ_MAX * sizeof(IconVtx)));
    memMk.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    icoMem_ = dkMemBlockCreate(&memMk);
    if (!icoMem_)
        return false;
    icoCpu_ = (IconVtx *)dkMemBlockGetCpuAddr(icoMem_);
    icoGpu_ = dkMemBlockGetGpuAddr(icoMem_);
    if (!icoCpu_ || icoGpu_ == DK_GPU_ADDR_INVALID)
        return false;

    /* Highlight ring queue: 64 quads max, 4K-rounded (neko3d rule). */
    dkMemBlockMakerDefaults(&memMk, device_,
                            (uint32_t)(((GFX_HL_MAX * 6 * sizeof(HighlightVtx) + 0xFFFU) & ~0xFFFU)));
    memMk.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    hlMem_ = dkMemBlockCreate(&memMk);
    if (!hlMem_)
        return false;
    hlCpu_ = (HighlightVtx *)dkMemBlockGetCpuAddr(hlMem_);
    hlGpu_ = dkMemBlockGetGpuAddr(hlMem_);
    if (!hlCpu_ || hlGpu_ == DK_GPU_ADDR_INVALID)
        return false;

    /* Image descriptors for GFX_MAX_TEX slots plus samplers. */
    dkMemBlockMakerDefaults(&memMk, device_, 0x4000);
    memMk.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    descMem_ = dkMemBlockCreate(&memMk);
    if (!descMem_)
        return false;
    descGpu_ = dkMemBlockGetGpuAddr(descMem_);
    if (!dkMemBlockGetCpuAddr(descMem_) || descGpu_ == DK_GPU_ADDR_INVALID)
        return false;
    ClearDescs();
    texUsed_[0] = texUsed_[1] = texUsed_[2] = true; /* font atlases */

    dkMemBlockMakerDefaults(&memMk, device_, 0x1000); /* neko3d aborts unless size is a 4K multiple */
    memMk.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    uboMem_ = dkMemBlockCreate(&memMk);
    if (!uboMem_)
        return false;
    uboGpu_ = dkMemBlockGetGpuAddr(uboMem_);
    if (!dkMemBlockGetCpuAddr(uboMem_) || uboGpu_ == DK_GPU_ADDR_INVALID)
        return false;
    {
        float *u = (float *)dkMemBlockGetCpuAddr(uboMem_);
        u[0] = (float)fbW_;
        u[1] = (float)fbH_;
    }

    DkCmdBufMaker cbMk;
    dkCmdBufMakerDefaults(&cbMk, device_);
    cmdbuf_ = dkCmdBufCreate(&cbMk);
    if (!cmdbuf_)
        return false;
    dkCmdBufAddMemory(cmdbuf_, cmdMem_, 0, GFX_CMD_SIZE);

    DkQueueMaker qMk;
    dkQueueMakerDefaults(&qMk, device_);
    qMk.flags = DkQueueFlags_Graphics;
    queue_ = dkQueueCreate(&qMk);
    if (!queue_)
        return false;

    ResetCounts();
    live_ = true;
    return true;
}

bool Gfx::LoadShaders()
{
    codeOff_ = 0;
    LoadOne(codeMem_, &codeOff_, &vsh_, quad_vsh_dksh, quad_vsh_dksh_size);
    LoadOne(codeMem_, &codeOff_, &fsh_, color_fsh_dksh, color_fsh_dksh_size);
    LoadOne(codeMem_, &codeOff_, &tvsh_, text_vsh_dksh, text_vsh_dksh_size);
    LoadOne(codeMem_, &codeOff_, &tfsh_, text_fsh_dksh, text_fsh_dksh_size);
    LoadOne(codeMem_, &codeOff_, &pvsh_, panel_vsh_dksh, panel_vsh_dksh_size);
    LoadOne(codeMem_, &codeOff_, &pfsh_, panel_fsh_dksh, panel_fsh_dksh_size);
    LoadOne(codeMem_, &codeOff_, &ivsh_, icon_vsh_dksh, icon_vsh_dksh_size);
    LoadOne(codeMem_, &codeOff_, &ifsh_, icon_fsh_dksh, icon_fsh_dksh_size);
    LoadOne(codeMem_, &codeOff_, &bfsh_, bg_fsh_dksh, bg_fsh_dksh_size);
    LoadOne(codeMem_, &codeOff_, &hvsh_, hl_vsh_dksh, hl_vsh_dksh_size);
    LoadOne(codeMem_, &codeOff_, &hfsh_, hl_fsh_dksh, hl_fsh_dksh_size);
    return codeOff_ <= GFX_CODE_SIZE;
}

void Gfx::Destroy()
{
    if (!live_)
        return;
    live_ = false;
    dkQueueWaitIdle(queue_);
    dkQueueDestroy(queue_);
    dkCmdBufDestroy(cmdbuf_);
    dkMemBlockDestroy(uboMem_);
    dkMemBlockDestroy(descMem_);
    dkMemBlockDestroy(icoMem_);
    dkMemBlockDestroy(hlMem_);
    dkMemBlockDestroy(pnlMem_);
    dkMemBlockDestroy(vtxMem_);
    dkMemBlockDestroy(cmdMem_);
    dkMemBlockDestroy(codeMem_);
    dkSwapchainDestroy(swapchain_);
    dkMemBlockDestroy(fbMem_);
    dkDeviceDestroy(device_);
    icoCpu_ = 0;
    hlCpu_ = 0;
    pnlCpu_ = 0;
    txtCpu_[0] = 0; txtCpu_[1] = 0; txtCpu_[2] = 0;
    vtxCpu_ = 0;
}

void Gfx::WaitIdle()
{
    if (live_)
        dkQueueWaitIdle(queue_);
}

int Gfx::TexAlloc()
{
    for (int i = 3; i < GFX_MAX_TEX; i++) {
        if (!texUsed_[i]) {
            texUsed_[i] = true;
            return i;
        }
    }
    return -1;
}

void Gfx::TexFree(int slot)
{
    if (slot >= 3 && slot < GFX_MAX_TEX)
        texUsed_[slot] = false;
}

void Gfx::WriteImageDesc(int slot, const DkImageView *view)
{
    uint8_t *dcpu = (uint8_t *)dkMemBlockGetCpuAddr(descMem_);
    if (!dcpu || slot < 0 || slot >= GFX_MAX_TEX)
        return;
    dkImageDescriptorInitialize((DkImageDescriptor *)(dcpu + slot * 0x20),
                                view, false, false);
}

void Gfx::WriteSamplerDesc()
{
    uint8_t *dcpu = (uint8_t *)dkMemBlockGetCpuAddr(descMem_);
    if (!dcpu)
        return;
    DkSampler sam;
    dkSamplerDefaults(&sam);
    sam.minFilter = DkFilter_Linear;
    sam.magFilter = DkFilter_Linear;
    sam.wrapMode[0] = DkWrapMode_ClampToEdge;
    sam.wrapMode[1] = DkWrapMode_ClampToEdge;
    sam.wrapMode[2] = DkWrapMode_ClampToEdge;
    dkSamplerDescriptorInitialize(
        (DkSamplerDescriptor *)(dcpu + GFX_MAX_TEX * 0x20), &sam);
}

void Gfx::ClearDescs()
{
    uint8_t *dcpu = (uint8_t *)dkMemBlockGetCpuAddr(descMem_);
    if (dcpu)
        memset(dcpu, 0, 0x4000);
}

void Gfx::PushQuad(float x, float y, float w, float h,
                   float r, float g, float b, float a)
{
    if (vtxCount_ + 6 > GFX_VTX_MAX)
        return;
    float X0 = X(x), Y0 = Y(y), X1 = X(x + w), Y1 = Y(y + h);
    HomeVtx *v = &vtxCpu_[vtxCount_];
    v[0].x = X0; v[0].y = Y0;
    v[1].x = X0; v[1].y = Y1;
    v[2].x = X1; v[2].y = Y0;
    v[3].x = X1; v[3].y = Y0;
    v[4].x = X0; v[4].y = Y1;
    v[5].x = X1; v[5].y = Y1;
    for (int i = 0; i < 6; i++) {
        v[i].r = r; v[i].g = g; v[i].b = b; v[i].a = a;
    }
    vtxCount_ += 6;
}

void Gfx::PushPanel(float x, float y, float w, float h, float rad,
                    float r, float g, float b, float a)
{
    if (pnlCount_ + 6 > GFX_PANEL_MAX)
        return;
    if (w <= 0.0f || h <= 0.0f)
        return;
    float X0 = X(x), Y0 = Y(y), X1 = X(x + w), Y1 = Y(y + h);
    float W = SC(w), H = SC(h), R = SC(rad);
    PanelVtx *v = &pnlCpu_[pnlCount_];
    v[0].x = X0; v[0].y = Y0;
    v[1].x = X0; v[1].y = Y1;
    v[2].x = X1; v[2].y = Y0;
    v[3].x = X1; v[3].y = Y0;
    v[4].x = X0; v[4].y = Y1;
    v[5].x = X1; v[5].y = Y1;
    for (int i = 0; i < 6; i++) {
        v[i].ox = X0; v[i].oy = Y0;
        v[i].w = W; v[i].h = H; v[i].rad = R; v[i].pad = 0.0f;
        v[i].r = r; v[i].g = g; v[i].b = b; v[i].a = a;
    }
    pnlCount_ += 6;
}

unsigned Gfx::PushIcon(float x, float y, float w, float h, float rad,
                       float r, float g, float b, float border, int tex, float brad,
                       float u0, float v0, float u1, float v1, float edgeBlend)
{
    if (icoCount_ + 6 > GFX_ICONQ_MAX)
        return 0xFFFFFFFFu;
    if (w <= 0.0f || h <= 0.0f)
        return 0xFFFFFFFFu;
    unsigned q = icoCount_ / 6;
    float X0 = X(x), Y0 = Y(y), X1 = X(x + w), Y1 = Y(y + h);
    float W = SC(w), H = SC(h), R = SC(rad);
    IconVtx *v = &icoCpu_[icoCount_];
    v[0].x = X0; v[0].y = Y0; v[0].u = u0; v[0].v = v0;
    v[1].x = X0; v[1].y = Y1; v[1].u = u0; v[1].v = v1;
    v[2].x = X1; v[2].y = Y0; v[2].u = u1; v[2].v = v0;
    v[3].x = X1; v[3].y = Y0; v[3].u = u1; v[3].v = v0;
    v[4].x = X0; v[4].y = Y1; v[4].u = u0; v[4].v = v1;
    v[5].x = X1; v[5].y = Y1; v[5].u = u1; v[5].v = v1;
    for (int i = 0; i < 6; i++) {
        v[i].ox = X0; v[i].oy = Y0;
        v[i].w = W; v[i].h = H; v[i].rad = R; v[i].pad = border;
        v[i].r = r; v[i].g = g; v[i].b = b; v[i].a = 1.0f;
        v[i].tslot = (float)tex;
        v[i].brad = (brad < 0.0f) ? rad : brad;
        v[i].edge = edgeBlend;
    }
    icoCount_ += 6;
    return q;
}

void Gfx::PushText(int buf, float x0, float y0, float x1, float y1,
                   float s0, float t0, float s1, float t1,
                   float r, float g, float b)
{
    if (buf < 0 || buf > 2 || txtCount_[buf] + 6 > GFX_TEXT_MAX)
        return;
    if (x1 <= x0 || y1 <= y0)
        return;
    float X0 = X(x0), Y0 = Y(y0), X1 = X(x1), Y1 = Y(y1);
    TextVtx *v = &txtCpu_[buf][txtCount_[buf]];
    v[0].x = X0; v[0].y = Y0; v[0].u = s0; v[0].v = t0;
    v[1].x = X0; v[1].y = Y1; v[1].u = s0; v[1].v = t1;
    v[2].x = X1; v[2].y = Y0; v[2].u = s1; v[2].v = t0;
    v[3].x = X1; v[3].y = Y0; v[3].u = s1; v[3].v = t0;
    v[4].x = X0; v[4].y = Y1; v[4].u = s0; v[4].v = t1;
    v[5].x = X1; v[5].y = Y1; v[5].u = s1; v[5].v = t1;
    for (int i = 0; i < 6; i++) {
        v[i].r = r; v[i].g = g; v[i].b = b; v[i].a = 1.0f;
    }
    txtCount_[buf] += 6;
}

void Gfx::BindUbo()
{
    dkCmdBufBindUniformBuffer(cmdbuf_, DkStage_Vertex, 0, uboGpu_, 16);
}

void Gfx::BindTexSets()
{
    dkCmdBufBindImageDescriptorSet(cmdbuf_, descGpu_, GFX_MAX_TEX);
    dkCmdBufBindSamplerDescriptorSet(cmdbuf_, descGpu_ + GFX_MAX_TEX * 0x20, 1);
    dkCmdBufBarrier(cmdbuf_, DkBarrier_None,
                    DkInvalidateFlags_Descriptors |
                    DkInvalidateFlags_Image |
                    DkInvalidateFlags_L2Cache);
}

bool Gfx::BeginFrame()
{
    int slot = dkQueueAcquireImage(queue_, swapchain_);
    if (slot < 0)
        return false;
    slot_ = slot;
    dkCmdBufClear(cmdbuf_);

    DkImageView view;
    dkImageViewDefaults(&view, &fbs_[slot]);
    dkCmdBufBindRenderTarget(cmdbuf_, &view, NULL);

    DkViewport vp = { 0.0f, 0.0f, (float)fbW_, (float)fbH_, 0.0f, 1.0f };
    DkScissor sc = { 0, 0, (uint32_t)fbW_, (uint32_t)fbH_ };
    dkCmdBufSetViewports(cmdbuf_, 0, &vp, 1);
    dkCmdBufSetScissors(cmdbuf_, 0, &sc, 1);
    return true;
}

void Gfx::DrawColor()
{
    if (vtxCount_ == 0)
        return;
    DkShader const *shaders[] = { &vsh_, &fsh_ };
    DkRasterizerState rast;
    DkColorWriteState colW;
    DkColorState col;
    dkRasterizerStateDefaults(&rast);
    dkColorWriteStateDefaults(&colW);
    dkColorStateDefaults(&col);
    dkCmdBufBindShaders(cmdbuf_, DkStageFlag_GraphicsMask, shaders, 2);
    dkCmdBufBindRasterizerState(cmdbuf_, &rast);
    dkCmdBufBindColorState(cmdbuf_, &col);
    dkCmdBufBindColorWriteState(cmdbuf_, &colW);
    BindUbo();

    DkVtxAttribState attribs[2];
    memset(attribs, 0, sizeof(attribs));
    attribs[0].bufferId = 0;
    attribs[0].offset = 0;
    attribs[0].size = DkVtxAttribSize_2x32;
    attribs[0].type = DkVtxAttribType_Float;
    attribs[1].bufferId = 0;
    attribs[1].offset = 8;
    attribs[1].size = DkVtxAttribSize_4x32;
    attribs[1].type = DkVtxAttribType_Float;
    DkVtxBufferState vtxState[1];
    vtxState[0].stride = sizeof(HomeVtx);
    vtxState[0].divisor = 0;
    dkCmdBufBindVtxAttribState(cmdbuf_, attribs, 2);
    dkCmdBufBindVtxBufferState(cmdbuf_, vtxState, 1);

    DkBufExtents vtxExt;
    vtxExt.addr = vtxGpu_;
    vtxExt.size = vtxCount_ * (uint32_t)sizeof(HomeVtx);
    dkCmdBufBindVtxBuffers(cmdbuf_, 0, &vtxExt, 1);
    dkCmdBufDraw(cmdbuf_, DkPrimitive_Triangles, vtxCount_, 1, 0, 0);
}

void Gfx::BlendedState(DkRasterizerState *rast, DkColorWriteState *colW,
                       DkColorState *tcol, DkBlendState *bl)
{
    dkRasterizerStateDefaults(rast);
    dkColorWriteStateDefaults(colW);
    dkColorStateDefaults(tcol);
    dkColorStateSetBlendEnable(tcol, 0, true);
    dkBlendStateDefaults(bl);
    dkBlendStateSetOps(bl, DkBlendOp_Add, DkBlendOp_Add);
    dkBlendStateSetFactors(bl, DkBlendFactor_SrcAlpha,
                           DkBlendFactor_InvSrcAlpha,
                           DkBlendFactor_SrcAlpha,
                           DkBlendFactor_InvSrcAlpha);
    dkCmdBufBindRasterizerState(cmdbuf_, rast);
    dkCmdBufBindColorState(cmdbuf_, tcol);
    dkCmdBufBindBlendState(cmdbuf_, 0, bl);
    dkCmdBufBindColorWriteState(cmdbuf_, colW);
}

void Gfx::DrawPanelsBg()
{
    if (pnlCount_ < 6)
        return;
    DkShader const *bsh[] = { &pvsh_, &bfsh_ };
    DkRasterizerState rast;
    DkColorWriteState colW;
    DkColorState tcol;
    DkBlendState bl;
    dkCmdBufBindShaders(cmdbuf_, DkStageFlag_GraphicsMask, bsh, 2);
    BlendedState(&rast, &colW, &tcol, &bl);
    BindUbo();
    BindPanelAttribs();
    DkBufExtents bgExt;
    bgExt.addr = pnlGpu_;
    bgExt.size = 6 * (uint32_t)sizeof(PanelVtx);
    dkCmdBufBindVtxBuffers(cmdbuf_, 0, &bgExt, 1);
    dkCmdBufDraw(cmdbuf_, DkPrimitive_Triangles, 6, 1, 0, 0);
}

void Gfx::BindPanelAttribs()
{
    DkVtxAttribState patt[4];
    memset(patt, 0, sizeof(patt));
    patt[0].bufferId = 0;
    patt[0].offset = 0;
    patt[0].size = DkVtxAttribSize_2x32;
    patt[0].type = DkVtxAttribType_Float;
    patt[1].bufferId = 0;
    patt[1].offset = 8;
    patt[1].size = DkVtxAttribSize_2x32;
    patt[1].type = DkVtxAttribType_Float;
    patt[2].bufferId = 0;
    patt[2].offset = 16;
    patt[2].size = DkVtxAttribSize_4x32;
    patt[2].type = DkVtxAttribType_Float;
    patt[3].bufferId = 0;
    patt[3].offset = 32;
    patt[3].size = DkVtxAttribSize_4x32;
    patt[3].type = DkVtxAttribType_Float;
    DkVtxBufferState pvtx[1];
    pvtx[0].stride = sizeof(PanelVtx);
    pvtx[0].divisor = 0;
    dkCmdBufBindVtxAttribState(cmdbuf_, patt, 4);
    dkCmdBufBindVtxBufferState(cmdbuf_, pvtx, 1);
}

void Gfx::DrawPanels()
{
    if (pnlCount_ <= 6)
        return;
    DkShader const *psh[] = { &pvsh_, &pfsh_ };
    DkRasterizerState rast;
    DkColorWriteState colW;
    DkColorState tcol;
    DkBlendState bl;
    dkCmdBufBindShaders(cmdbuf_, DkStageFlag_GraphicsMask, psh, 2);
    BlendedState(&rast, &colW, &tcol, &bl);
    BindUbo();
    BindPanelAttribs();
    DkBufExtents pnExt;
    pnExt.addr = pnlGpu_ + 6 * (uint32_t)sizeof(PanelVtx);
    pnExt.size = (pnlCount_ - 6) * (uint32_t)sizeof(PanelVtx);
    dkCmdBufBindVtxBuffers(cmdbuf_, 0, &pnExt, 1);
    dkCmdBufDraw(cmdbuf_, DkPrimitive_Triangles, pnlCount_ - 6, 1, 0, 0);
}

void Gfx::DrawIcons()
{
    unsigned nq = icoCount_ / 6;
    if (nq == 0)
        return;
    DkShader const *tsh[] = { &ivsh_, &ifsh_ };
    DkRasterizerState rast;
    DkColorWriteState colW;
    DkColorState tcol;
    DkBlendState bl;
    dkCmdBufBindShaders(cmdbuf_, DkStageFlag_GraphicsMask, tsh, 2);
    BlendedState(&rast, &colW, &tcol, &bl);
    BindUbo();
    BindTexSets();

    DkVtxAttribState iatt[8];
    memset(iatt, 0, sizeof(iatt));
    iatt[0].bufferId = 0;
    iatt[0].offset = 0;
    iatt[0].size = DkVtxAttribSize_2x32;
    iatt[0].type = DkVtxAttribType_Float;
    iatt[1].bufferId = 0;
    iatt[1].offset = 8;
    iatt[1].size = DkVtxAttribSize_2x32;
    iatt[1].type = DkVtxAttribType_Float;
    iatt[2].bufferId = 0;
    iatt[2].offset = 16;
    iatt[2].size = DkVtxAttribSize_2x32;
    iatt[2].type = DkVtxAttribType_Float;
    iatt[3].bufferId = 0;
    iatt[3].offset = 24;
    iatt[3].size = DkVtxAttribSize_4x32;
    iatt[3].type = DkVtxAttribType_Float;
    iatt[4].bufferId = 0;
    iatt[4].offset = 40;
    iatt[4].size = DkVtxAttribSize_4x32;
    iatt[4].type = DkVtxAttribType_Float;
    iatt[5].bufferId = 0;
    iatt[5].offset = 56;
    iatt[5].size = DkVtxAttribSize_1x32;
    iatt[5].type = DkVtxAttribType_Float;
    iatt[6].bufferId = 0;
    iatt[6].offset = 60;
    iatt[6].size = DkVtxAttribSize_1x32;
    iatt[6].type = DkVtxAttribType_Float;
    iatt[7].bufferId = 0;
    iatt[7].offset = 64;
    iatt[7].size = DkVtxAttribSize_1x32;
    iatt[7].type = DkVtxAttribType_Float;
    DkVtxBufferState ivtx[1];
    ivtx[0].stride = sizeof(IconVtx);
    ivtx[0].divisor = 0;
    dkCmdBufBindVtxAttribState(cmdbuf_, iatt, 8);
    dkCmdBufBindVtxBufferState(cmdbuf_, ivtx, 1);

    for (unsigned j = 0; j < nq; j++) {
        int tslot = (int)icoCpu_[j * 6].tslot;
        if (tslot <= 0 || tslot >= GFX_MAX_TEX)
            continue;
        DkBufExtents ixExt;
        ixExt.addr = icoGpu_ + j * 6 * (uint32_t)sizeof(IconVtx);
        ixExt.size = 6 * (uint32_t)sizeof(IconVtx);
        dkCmdBufBindVtxBuffers(cmdbuf_, 0, &ixExt, 1);
        dkCmdBufBindTexture(cmdbuf_, DkStage_Fragment, 0,
                            dkMakeTextureHandle((uint32_t)tslot, 0));
        dkCmdBufDraw(cmdbuf_, DkPrimitive_Triangles, 6, 1, 0, 0);
    }
}

void Gfx::DrawTextBuf(int texSlot, int buf)
{
    if (buf < 0 || buf > 2 || txtCount_[buf] == 0)
        return;
    DkShader const *ssh[] = { &tvsh_, &tfsh_ };
    DkRasterizerState rast;
    DkColorWriteState colW;
    DkColorState tcol;
    DkBlendState bl;
    dkCmdBufBindShaders(cmdbuf_, DkStageFlag_GraphicsMask, ssh, 2);
    BlendedState(&rast, &colW, &tcol, &bl);
    BindUbo();
    BindTexSets();

    DkVtxAttribState tatt[3];
    memset(tatt, 0, sizeof(tatt));
    tatt[0].bufferId = 0;
    tatt[0].offset = 0;
    tatt[0].size = DkVtxAttribSize_2x32;
    tatt[0].type = DkVtxAttribType_Float;
    tatt[1].bufferId = 0;
    tatt[1].offset = 8;
    tatt[1].size = DkVtxAttribSize_2x32;
    tatt[1].type = DkVtxAttribType_Float;
    tatt[2].bufferId = 0;
    tatt[2].offset = 16;
    tatt[2].size = DkVtxAttribSize_4x32;
    tatt[2].type = DkVtxAttribType_Float;
    DkVtxBufferState tvtx[1];
    tvtx[0].stride = sizeof(TextVtx);
    tvtx[0].divisor = 0;
    dkCmdBufBindVtxAttribState(cmdbuf_, tatt, 3);
    dkCmdBufBindVtxBufferState(cmdbuf_, tvtx, 1);

    DkBufExtents textExt;
    textExt.addr = txtGpu_[buf];
    textExt.size = txtCount_[buf] * (uint32_t)sizeof(TextVtx);
    dkCmdBufBindVtxBuffers(cmdbuf_, 0, &textExt, 1);
    dkCmdBufBindTexture(cmdbuf_, DkStage_Fragment, 0,
                        dkMakeTextureHandle((uint32_t)texSlot, 0));
    dkCmdBufDraw(cmdbuf_, DkPrimitive_Triangles, txtCount_[buf], 1, 0, 0);
}

void Gfx::EndFrame()
{
    DkCmdList list = dkCmdBufFinishList(cmdbuf_);
    dkQueueSubmitCommands(queue_, list);
    dkQueuePresentImage(queue_, swapchain_, slot_);
}


bool Gfx::Ready() { return live_; }
float Gfx::X(float x) { return ox_ + x * s_; }
float Gfx::Y(float y) { return oy_ + y * s_; }
float Gfx::SC(float v) { return v * s_; }
static float s_ringRot = 0.0f;

void Gfx::PushSelectRing(float x, float y, float w, float h, float rad, float thick)
{
    /* Dynamic selection border (mockup Gradient Outline colors). */
    static const float c0[3] = { 0.537f, 0.584f, 0.945f };
    static const float c1[3] = { 0.384f, 0.588f, 0.949f };
    static const float c2[3] = { 0.863f, 0.722f, 0.875f };
    static const float c3[3] = { 1.0f, 1.0f, 1.0f };
    PushHighlight(x, y, w, h, rad, thick, c0, c1, c2, c3, s_ringRot);
}

void Gfx::PushHighlight(float x, float y, float w, float h, float rad,
                           float thick, const float *c0, const float *c1,
                           const float *c2, const float *c3, float rot)
{
    if (hlCount_ + 6 > GFX_HL_MAX * 6)
        return;
    if (w <= 0.0f || h <= 0.0f || thick <= 0.0f)
        return;
    float X0 = X(x), Y0 = Y(y), X1 = X(x + w), Y1 = Y(y + h);
    float W = SC(w), H = SC(h), R = SC(rad), T = SC(thick);
    HighlightVtx *v = &hlCpu_[hlCount_];
    v[0].x = X0; v[0].y = Y0;
    v[1].x = X0; v[1].y = Y1;
    v[2].x = X1; v[2].y = Y0;
    v[3].x = X1; v[3].y = Y0;
    v[4].x = X0; v[4].y = Y1;
    v[5].x = X1; v[5].y = Y1;
    for (int i = 0; i < 6; i++) {
        v[i].ox = X0; v[i].oy = Y0;
        v[i].w = W; v[i].h = H; v[i].rad = R; v[i].thick = T;
        v[i].c0r = c0[0]; v[i].c0g = c0[1]; v[i].c0b = c0[2]; v[i].c0a = 1.0f;
        v[i].c1r = c1[0]; v[i].c1g = c1[1]; v[i].c1b = c1[2]; v[i].c1a = 1.0f;
        v[i].c2r = c2[0]; v[i].c2g = c2[1]; v[i].c2b = c2[2]; v[i].c2a = 1.0f;
        v[i].c3r = c3[0]; v[i].c3g = c3[1]; v[i].c3b = c3[2]; v[i].c3a = 1.0f;
        v[i].rot = rot;
    }
    hlCount_ += 6;
}

void Gfx::BindHlAttribs()
{
    DkVtxAttribState hatt[8];
    memset(hatt, 0, sizeof(hatt));
    hatt[0].bufferId = 0;
    hatt[0].offset = 0;
    hatt[0].size = DkVtxAttribSize_2x32;
    hatt[0].type = DkVtxAttribType_Float;
    hatt[1].bufferId = 0;
    hatt[1].offset = 8;
    hatt[1].size = DkVtxAttribSize_2x32;
    hatt[1].type = DkVtxAttribType_Float;
    hatt[2].bufferId = 0;
    hatt[2].offset = 16;
    hatt[2].size = DkVtxAttribSize_4x32;
    hatt[2].type = DkVtxAttribType_Float;
    hatt[3].bufferId = 0;
    hatt[3].offset = 32;
    hatt[3].size = DkVtxAttribSize_4x32;
    hatt[3].type = DkVtxAttribType_Float;
    hatt[4].bufferId = 0;
    hatt[4].offset = 48;
    hatt[4].size = DkVtxAttribSize_4x32;
    hatt[4].type = DkVtxAttribType_Float;
    hatt[5].bufferId = 0;
    hatt[5].offset = 64;
    hatt[5].size = DkVtxAttribSize_4x32;
    hatt[5].type = DkVtxAttribType_Float;
    hatt[6].bufferId = 0;
    hatt[6].offset = 80;
    hatt[6].size = DkVtxAttribSize_4x32;
    hatt[6].type = DkVtxAttribType_Float;
    hatt[7].bufferId = 0;
    hatt[7].offset = 96;
    hatt[7].size = DkVtxAttribSize_1x32;
    hatt[7].type = DkVtxAttribType_Float;
    DkVtxBufferState hvtx[1];
    hvtx[0].stride = sizeof(HighlightVtx);
    hvtx[0].divisor = 0;
    dkCmdBufBindVtxAttribState(cmdbuf_, hatt, 8);
    dkCmdBufBindVtxBufferState(cmdbuf_, hvtx, 1);
}

void Gfx::DrawHighlights()
{
    if (hlCount_ == 0)
        return;
    DkShader const *hsh[] = { &hvsh_, &hfsh_ };
    DkRasterizerState rast;
    DkColorWriteState colW;
    DkColorState tcol;
    DkBlendState bl;
    dkCmdBufBindShaders(cmdbuf_, DkStageFlag_GraphicsMask, hsh, 2);
    BlendedState(&rast, &colW, &tcol, &bl);
    BindUbo();
    BindHlAttribs();
    DkBufExtents hlExt;
    hlExt.addr = hlGpu_;
    hlExt.size = hlCount_ * (uint32_t)sizeof(HighlightVtx);
    dkCmdBufBindVtxBuffers(cmdbuf_, 0, &hlExt, 1);
    dkCmdBufDraw(cmdbuf_, DkPrimitive_Triangles, hlCount_, 1, 0, 0);
}

void Gfx::ResetCounts()
{
    s_ringRot += 0.5f / 60.0f;
    if (s_ringRot >= 1.0f)
        s_ringRot -= 1.0f;
    vtxCount_ = 0;
    txtCount_[0] = 0;
    txtCount_[1] = 0;
    txtCount_[2] = 0;
    pnlCount_ = 0;
    icoCount_ = 0;
    hlCount_ = 0;
}
unsigned Gfx::IconQuads() { return icoCount_ / 6; }
DkDevice Gfx::Device() { return device_; }
