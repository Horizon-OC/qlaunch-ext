/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include <string.h>
#include <switch.h>
#include <deko3d.h>

#define STBTT_assert(x)
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

int qext_refresh_titles(void);
int qext_title_count(void);
u64 qext_title_id(int index);
int qext_title_name(int index, char *out, unsigned out_cap);
Result qext_launch_title(u64 tid);
Result qext_resume_game(void);
Result qext_terminate_game(void);
int qext_game_running(void);
int qext_game_has_foreground(void);
u64 qext_suspended_title(void);
Result qext_launch_applet(int kind); /* 0 album */

#define FB_NUM 2
#define FB_WIDTH 1280
#define FB_HEIGHT 720
#define CODEMEMSIZE (64 * 1024)
#define CMDMEMSIZE (64 * 1024)
#define VTX_MAX 2048
#define TEXT_MAX 8192

extern const uint8_t quad_vsh_dksh[];
extern const uint32_t quad_vsh_dksh_size;
extern const uint8_t color_fsh_dksh[];
extern const uint32_t color_fsh_dksh_size;
extern const uint8_t text_vsh_dksh[];
extern const uint32_t text_vsh_dksh_size;
extern const uint8_t text_fsh_dksh[];
extern const uint32_t text_fsh_dksh_size;
extern const uint8_t Lexend_Regular_ttf[];
extern const uint32_t Lexend_Regular_ttf_size;

typedef struct { float x, y, r, g, b, a; } HomeVtx;
typedef struct { float x, y, u, v, r, g, b, a; } TextVtx;

#define MENU_MAX_TITLES 64
#define ROW_NAME_MAX 192
#define APPLET_ROWS 1

typedef struct {
    bool is_game;
    u64 tid;      /* games: title id */
    int applet;   /* applets: 0 album */
    char name[ROW_NAME_MAX];
} Row;

static DkDevice s_device;
static DkMemBlock s_fbMem;
static DkImage s_fbs[FB_NUM];
static DkSwapchain s_swapchain;
static DkMemBlock s_codeMem;
static uint32_t s_codeOff;
static DkShader s_vsh;
static DkShader s_fsh;
static DkShader s_tvsh;
static DkShader s_tfsh;
static DkMemBlock s_cmdMem;
static DkMemBlock s_vtxMem;
static HomeVtx *s_vtxCpu;
static DkGpuAddr s_vtxGpu;
static unsigned s_vtxCount;
static TextVtx *s_txtCpu;
static DkGpuAddr s_txtGpu;
static unsigned s_txtCount;
static DkCmdBuf s_cmdbuf;
static DkQueue s_queue;
static bool s_live;

#define FONT_ATLAS_W 1024
#define FONT_ATLAS_H 1024
#define FONT_FIRST 32
#define FONT_COUNT 224
static uint8_t s_atlas[FONT_ATLAS_W * FONT_ATLAS_H];
static stbtt_bakedchar s_glyphs[FONT_COUNT];
static float s_fontPx;
static bool s_fontOk;
static DkMemBlock s_fontMem;
static DkImage s_fontImg;
static DkImageView s_fontView;
static DkMemBlock s_descMem;
static DkGpuAddr s_descGpu;

static PadState s_pad;
static bool s_padReady;

static Row s_rows[MENU_MAX_TITLES + APPLET_ROWS];
static int s_rowCount;
static int s_sel;
static unsigned s_holdFrames;
static bool s_launchBlack;
static unsigned s_frame;

static void push_vert(float x, float y, float r, float g, float b, float a)
{
    if (s_vtxCount >= VTX_MAX)
        return;
    HomeVtx *v = &s_vtxCpu[s_vtxCount++];
    v->x = x; v->y = y; v->r = r; v->g = g; v->b = b; v->a = a;
}

static void push_quad(float x, float y, float w, float h,
                      float r, float g, float b, float a)
{
    push_vert(x, y, r, g, b, a);
    push_vert(x, y + h, r, g, b, a);
    push_vert(x + w, y, r, g, b, a);
    push_vert(x + w, y, r, g, b, a);
    push_vert(x, y + h, r, g, b, a);
    push_vert(x + w, y + h, r, g, b, a);
}

static void push_text_quad(float x0, float y0, float x1, float y1,
                           float s0, float t0, float s1, float t1,
                           float r, float g, float b)
{
    if (s_txtCount + 6 > TEXT_MAX)
        return;
    if (x1 <= x0 || y1 <= y0)
        return;
    TextVtx *v = &s_txtCpu[s_txtCount];
    v[0].x = x0; v[0].y = y0; v[0].u = s0; v[0].v = t0;
    v[1].x = x0; v[1].y = y1; v[1].u = s0; v[1].v = t1;
    v[2].x = x1; v[2].y = y0; v[2].u = s1; v[2].v = t0;
    v[3].x = x1; v[3].y = y0; v[3].u = s1; v[3].v = t0;
    v[4].x = x0; v[4].y = y1; v[4].u = s0; v[4].v = t1;
    v[5].x = x1; v[5].y = y1; v[5].u = s1; v[5].v = t1;
    for (int i = 0; i < 6; i++) {
        v[i].r = r; v[i].g = g; v[i].b = b; v[i].a = 1.0f;
    }
    s_txtCount += 6;
}

static uint32_t utf8_next(const char **pp)
{
    const uint8_t *p = (const uint8_t *)*pp;
    uint8_t c = *p;
    if (c < 0x80) {
        *pp = (const char *)(p + 1);
        return c;
    }
    uint32_t cp;
    int need;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; need = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; need = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; need = 3; }
    else { *pp = (const char *)(p + 1); return 0xFFFD; }
    for (int i = 0; i < need; i++) {
        uint8_t d = p[1 + i];
        if ((d & 0xC0) != 0x80) {
            *pp = (const char *)(p + 1);
            return 0xFFFD;
        }
        cp = (cp << 6) | (d & 0x3F);
    }
    *pp = (const char *)(p + 1 + need);
    return cp;
}

static void reload_rows(void)
{
    static const char fallback[] = "Unknown title";
    u64 keep_tid = 0;
    int keep_applet = -2;
    bool keep_game = true;
    if (s_sel >= 0 && s_sel < s_rowCount) {
        keep_game = s_rows[s_sel].is_game;
        keep_tid = s_rows[s_sel].tid;
        keep_applet = s_rows[s_sel].applet;
    }
    int n = qext_title_count();
    if (n < 0)
        n = 0;
    if (n > MENU_MAX_TITLES)
        n = MENU_MAX_TITLES;
    s_rowCount = 0;
    for (int i = 0; i < n; i++) {
        Row *r = &s_rows[s_rowCount++];
        r->is_game = true;
        r->tid = qext_title_id(i);
        r->applet = -1;
        r->name[0] = 0;
        int len = qext_title_name(i, r->name, ROW_NAME_MAX);
        if (len < 0 || r->name[0] == 0) {
            unsigned k = 0;
            while (fallback[k] && k + 1 < ROW_NAME_MAX) {
                r->name[k] = fallback[k];
                k++;
            }
            r->name[k] = 0;
        } else {
            r->name[ROW_NAME_MAX - 1] = 0;
        }
    }
    Row *a = &s_rows[s_rowCount++];
    a->is_game = false;
    a->tid = 0;
    a->applet = 0;
    memcpy(a->name, "Album", 6);
    /* Keep cursor on the same title across refreshes (new cards, etc). */
    int want = -1;
    for (int i = 0; i < s_rowCount; i++) {
        if (s_rows[i].is_game == keep_game && s_rows[i].tid == keep_tid &&
            s_rows[i].applet == keep_applet) {
            want = i;
            break;
        }
    }
    if (want >= 0)
        s_sel = want;
    else if (s_sel >= s_rowCount)
        s_sel = s_rowCount > 0 ? s_rowCount - 1 : 0;
    if (s_sel < 0)
        s_sel = 0;
}

static bool font_bake(void)
{
    static const float heights[] = { 64.0f, 56.0f, 48.0f, 40.0f };
    for (unsigned i = 0; i < 4; i++) {
        memset(s_atlas, 0, sizeof(s_atlas));
        int rows = stbtt_BakeFontBitmap(Lexend_Regular_ttf, 0, heights[i],
            s_atlas, FONT_ATLAS_W, FONT_ATLAS_H,
            FONT_FIRST, FONT_COUNT, s_glyphs);
        if (rows > 0 && rows <= FONT_ATLAS_H) {
            s_fontPx = heights[i];
            return true;
        }
    }
    s_fontPx = 0.0f;
    return false;
}

static void write_descriptors(uint8_t *dcpu)
{
    dkImageDescriptorInitialize((DkImageDescriptor *)dcpu, &s_fontView, false, false);
    DkSampler sam;
    dkSamplerDefaults(&sam);
    sam.minFilter = DkFilter_Linear;
    sam.magFilter = DkFilter_Linear;
    sam.wrapMode[0] = DkWrapMode_ClampToEdge;
    sam.wrapMode[1] = DkWrapMode_ClampToEdge;
    sam.wrapMode[2] = DkWrapMode_ClampToEdge;
    dkSamplerDescriptorInitialize((DkSamplerDescriptor *)(dcpu + 0x20), &sam);
}

static bool font_init(void)
{
    if (!font_bake())
        return false;

    DkImageLayoutMaker lm;
    dkImageLayoutMakerDefaults(&lm, s_device);
    lm.flags = DkImageFlags_PitchLinear;
    lm.format = DkImageFormat_R8_Unorm;
    lm.dimensions[0] = FONT_ATLAS_W;
    lm.dimensions[1] = FONT_ATLAS_H;
    DkImageLayout lay;
    dkImageLayoutInitialize(&lay, &lm);
    uint32_t sz = dkImageLayoutGetSize(&lay);
    uint32_t al = dkImageLayoutGetAlignment(&lay);
    sz = (sz + al - 1) & ~(al - 1);

    DkMemBlockMaker mm;
    dkMemBlockMakerDefaults(&mm, s_device, sz);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    s_fontMem = dkMemBlockCreate(&mm);
    if (!s_fontMem)
        return false;
    memcpy(dkMemBlockGetCpuAddr(s_fontMem), s_atlas,
           (size_t)FONT_ATLAS_W * (size_t)FONT_ATLAS_H);
    dkImageInitialize(&s_fontImg, &lay, s_fontMem, 0);

    dkImageViewDefaults(&s_fontView, &s_fontImg);
    s_fontView.swizzle[0] = DkImageSwizzle_One;
    s_fontView.swizzle[1] = DkImageSwizzle_One;
    s_fontView.swizzle[2] = DkImageSwizzle_One;
    s_fontView.swizzle[3] = DkImageSwizzle_Red;

    dkMemBlockMakerDefaults(&mm, s_device, 0x1000);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    s_descMem = dkMemBlockCreate(&mm);
    if (!s_descMem)
        return false;
    s_descGpu = dkMemBlockGetGpuAddr(s_descMem);
    uint8_t *dcpu = (uint8_t *)dkMemBlockGetCpuAddr(s_descMem);
    if (!dcpu || s_descGpu == DK_GPU_ADDR_INVALID)
        return false;
    write_descriptors(dcpu);
    return true;
}

static void font_refresh(void)
{
    if (!s_fontOk)
        return;
    memcpy(dkMemBlockGetCpuAddr(s_fontMem), s_atlas,
           (size_t)FONT_ATLAS_W * (size_t)FONT_ATLAS_H);
    uint8_t *dcpu = (uint8_t *)dkMemBlockGetCpuAddr(s_descMem);
    if (dcpu)
        write_descriptors(dcpu);
}

#define PX_NORM 28.0f
#define PX_SEL 40.0f
#define PX_ACTIVE 52.0f

static float draw_text(const char *s, float x, float y, float px,
                       float r, float g, float b)
{
    if (!s || s_fontPx <= 0.0f)
        return x;
    float scale = px / s_fontPx;
    float pen = x;
    while (*s) {
        uint32_t cp = utf8_next(&s);
        if (cp < 32u || cp > 255u)
            cp = 63u;
        if (pen > (float)(FB_WIDTH - 16))
            break;
        float before = pen;
        float xpos = pen;
        float ypos = y;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(s_glyphs, FONT_ATLAS_W, FONT_ATLAS_H,
                           (int)(cp - 32u), &xpos, &ypos, &q, 1);
        float x0 = before + (q.x0 - before) * scale;
        float x1 = before + (q.x1 - before) * scale;
        float y0 = y + (q.y0 - y) * scale;
        float y1 = y + (q.y1 - y) * scale;
        pen = before + (xpos - before) * scale;
        if (x1 > x0 && y1 > y0)
            push_text_quad(x0, y0, x1, y1, q.s0, q.t0, q.s1, q.t1, r, g, b);
    }
    return pen;
}

#define ROW_X 96.0f
#define ROW_Y0 84.0f
#define ROW_STRIDE 78.0f
#define ROW_VISIBLE 8

static void draw_frame(bool black_only)
{
    s_vtxCount = 0;
    s_txtCount = 0;
    push_quad(0.0f, 0.0f, (float)FB_WIDTH, (float)FB_HEIGHT,
              0.0f, 0.0f, 0.0f, 1.0f);
    if (black_only)
        return;
    if (s_rowCount <= 0) {
        draw_text("No titles found", ROW_X, 300.0f, PX_NORM, 0.6f, 0.6f, 0.65f);
        return;
    }
    u64 susp = qext_suspended_title();
    bool running = susp != 0 && qext_game_running();
    int w0 = 0;
    if (s_rowCount > ROW_VISIBLE) {
        w0 = s_sel - ROW_VISIBLE / 2;
        if (w0 < 0)
            w0 = 0;
        if (w0 > s_rowCount - ROW_VISIBLE)
            w0 = s_rowCount - ROW_VISIBLE;
    }
    int w1 = w0 + ROW_VISIBLE;
    if (w1 > s_rowCount)
        w1 = s_rowCount;
    for (int i = w0; i < w1; i++) {
        Row *r = &s_rows[i];
        float top = ROW_Y0 + (float)(i - w0) * ROW_STRIDE;
        bool active = r->is_game && running && r->tid == susp;
        float px = active ? PX_ACTIVE : (i == s_sel ? PX_SEL : PX_NORM);
        float cr = 1.0f, cg = 1.0f, cb = 1.0f;
        if (!active && i != s_sel) {
            cr = 0.60f; cg = 0.62f; cb = 0.68f;
        }
        float base = top + (ROW_STRIDE - px) * 0.5f + px * 0.8f;
        draw_text(r->name, ROW_X, base, px, cr, cg, cb);
    }
}

static void submit_frame(void)
{
    int slot = dkQueueAcquireImage(s_queue, s_swapchain);
    if (slot < 0)
        return;

    dkCmdBufClear(s_cmdbuf);

    DkImageView view;
    dkImageViewDefaults(&view, &s_fbs[slot]);
    dkCmdBufBindRenderTarget(s_cmdbuf, &view, NULL);

    DkViewport vp = { 0.0f, 0.0f, (float)FB_WIDTH, (float)FB_HEIGHT, 0.0f, 1.0f };
    DkScissor sc = { 0, 0, FB_WIDTH, FB_HEIGHT };
    dkCmdBufSetViewports(s_cmdbuf, 0, &vp, 1);
    dkCmdBufSetScissors(s_cmdbuf, 0, &sc, 1);

    DkRasterizerState rast;
    DkColorWriteState colW;
    dkRasterizerStateDefaults(&rast);
    dkColorWriteStateDefaults(&colW);

    /* Solid color pass (background). */
    {
        DkShader const *shaders[] = { &s_vsh, &s_fsh };
        DkColorState col;
        dkColorStateDefaults(&col);
        dkCmdBufBindShaders(s_cmdbuf, DkStageFlag_GraphicsMask, shaders, 2);
        dkCmdBufBindRasterizerState(s_cmdbuf, &rast);
        dkCmdBufBindColorState(s_cmdbuf, &col);
        dkCmdBufBindColorWriteState(s_cmdbuf, &colW);

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
        dkCmdBufBindVtxAttribState(s_cmdbuf, attribs, 2);
        dkCmdBufBindVtxBufferState(s_cmdbuf, vtxState, 1);

        DkBufExtents vtxExt;
        vtxExt.addr = s_vtxGpu;
        vtxExt.size = s_vtxCount * (uint32_t)sizeof(HomeVtx);
        dkCmdBufBindVtxBuffers(s_cmdbuf, 0, &vtxExt, 1);

        dkCmdBufDraw(s_cmdbuf, DkPrimitive_Triangles, s_vtxCount, 1, 0, 0);
    }

    if (s_txtCount > 0 && s_fontOk) {
        DkShader const *tsh[] = { &s_tvsh, &s_tfsh };
        DkColorState tcol;
        dkColorStateDefaults(&tcol);
        dkColorStateSetBlendEnable(&tcol, 0, true);
        DkBlendState bl;
        dkBlendStateDefaults(&bl);
        dkBlendStateSetOps(&bl, DkBlendOp_Add, DkBlendOp_Add);
        dkBlendStateSetFactors(&bl, DkBlendFactor_SrcAlpha,
                               DkBlendFactor_InvSrcAlpha,
                               DkBlendFactor_SrcAlpha,
                               DkBlendFactor_InvSrcAlpha);
        dkCmdBufBindShaders(s_cmdbuf, DkStageFlag_GraphicsMask, tsh, 2);
        dkCmdBufBindRasterizerState(s_cmdbuf, &rast);
        dkCmdBufBindColorState(s_cmdbuf, &tcol);
        dkCmdBufBindBlendState(s_cmdbuf, 0, &bl);
        dkCmdBufBindColorWriteState(s_cmdbuf, &colW);

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
        dkCmdBufBindVtxAttribState(s_cmdbuf, tatt, 3);
        dkCmdBufBindVtxBufferState(s_cmdbuf, tvtx, 1);

        DkBufExtents textExt;
        textExt.addr = s_txtGpu;
        textExt.size = s_txtCount * (uint32_t)sizeof(TextVtx);
        dkCmdBufBindVtxBuffers(s_cmdbuf, 0, &textExt, 1);

        dkCmdBufBindImageDescriptorSet(s_cmdbuf, s_descGpu, 1);
        dkCmdBufBindSamplerDescriptorSet(s_cmdbuf, s_descGpu + 0x20, 1);
        dkCmdBufBarrier(s_cmdbuf, DkBarrier_None, DkInvalidateFlags_Descriptors | DkInvalidateFlags_Image | DkInvalidateFlags_L2Cache);
        dkCmdBufBindTexture(s_cmdbuf, DkStage_Fragment, 0, dkMakeTextureHandle(0, 0));

        dkCmdBufDraw(s_cmdbuf, DkPrimitive_Triangles, s_txtCount, 1, 0, 0);
    }

    DkCmdList list = dkCmdBufFinishList(s_cmdbuf);

    dkQueueSubmitCommands(s_queue, list);
    dkQueuePresentImage(s_queue, s_swapchain, slot);
}

static void blackout(void)
{
    draw_frame(true);
    submit_frame();
    draw_frame(true);
    submit_frame();
}

static void activate(void)
{
    if (s_sel < 0 || s_sel >= s_rowCount)
        return;
    Row *r = &s_rows[s_sel];
    if (r->is_game) {
        u64 susp = qext_suspended_title();
        if (susp != 0 && susp == r->tid && qext_game_running() &&
            !qext_game_has_foreground()) {
            s_launchBlack = true;
            blackout();
            qext_resume_game();
        } else if (!qext_game_has_foreground()) {
            s_launchBlack = true;
            blackout();
            qext_launch_title(r->tid);
        }
    } else {
        qext_launch_applet(r->applet);
    }
}

static void handle_input(void)
{
    if (!s_padReady)
        return;
    padUpdate(&s_pad);
    u64 down = padGetButtonsDown(&s_pad);
    u64 held = padGetButtons(&s_pad);
    bool moved = false;
    if ((down & HidNpadButton_AnyUp) && s_sel > 0) {
        s_sel--;
        moved = true;
    }
    if ((down & HidNpadButton_AnyDown) && s_sel < s_rowCount - 1) {
        s_sel++;
        moved = true;
    }
    if (!moved && s_rowCount > 0) {
        if (held & (HidNpadButton_AnyUp | HidNpadButton_AnyDown)) {
            s_holdFrames++;
            if (s_holdFrames > 20 && (s_holdFrames % 6) == 0) {
                if ((held & HidNpadButton_AnyUp) && s_sel > 0)
                    s_sel--;
                if ((held & HidNpadButton_AnyDown) && s_sel < s_rowCount - 1)
                    s_sel++;
            }
        } else {
            s_holdFrames = 0;
        }
    } else {
        s_holdFrames = 0;
    }
    if (down & HidNpadButton_A)
        activate();
    if (down & HidNpadButton_X)
        qext_terminate_game();
    if (down & HidNpadButton_Plus) {
        qext_refresh_titles();
        reload_rows();
    }
}

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
    load_embedded(&s_vsh, quad_vsh_dksh, quad_vsh_dksh_size);
    load_embedded(&s_fsh, color_fsh_dksh, color_fsh_dksh_size);
    load_embedded(&s_tvsh, text_vsh_dksh, text_vsh_dksh_size);
    load_embedded(&s_tfsh, text_fsh_dksh, text_fsh_dksh_size);

    dkMemBlockMakerDefaults(&memMk, s_device, CMDMEMSIZE);
    memMk.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    s_cmdMem = dkMemBlockCreate(&memMk);
    if (!s_cmdMem)
        return -5;

    uint32_t colorBytes = (uint32_t)(VTX_MAX * sizeof(HomeVtx));
    uint32_t textBytes = (uint32_t)(TEXT_MAX * sizeof(TextVtx));
    dkMemBlockMakerDefaults(&memMk, s_device, colorBytes + textBytes);
    memMk.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    s_vtxMem = dkMemBlockCreate(&memMk);
    if (!s_vtxMem)
        return -6;
    s_vtxCpu = (HomeVtx *)dkMemBlockGetCpuAddr(s_vtxMem);
    s_vtxGpu = dkMemBlockGetGpuAddr(s_vtxMem);
    if (!s_vtxCpu || s_vtxGpu == DK_GPU_ADDR_INVALID)
        return -6;
    s_txtCpu = (TextVtx *)((uint8_t *)s_vtxCpu + colorBytes);
    s_txtGpu = s_vtxGpu + colorBytes;
    s_vtxCount = 0;
    s_txtCount = 0;

    s_fontOk = font_init();
    if (!s_fontOk)
        return -9;

    DkCmdBufMaker cbMk;
    dkCmdBufMakerDefaults(&cbMk, s_device);
    s_cmdbuf = dkCmdBufCreate(&cbMk);
    if (!s_cmdbuf)
        return -7;
    dkCmdBufAddMemory(s_cmdbuf, s_cmdMem, 0, CMDMEMSIZE);

    DkQueueMaker qMk;
    dkQueueMakerDefaults(&qMk, s_device);
    qMk.flags = DkQueueFlags_Graphics;
    s_queue = dkQueueCreate(&qMk);
    if (!s_queue)
        return -8;

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&s_pad);
    s_padReady = true;

    s_sel = 0;
    s_holdFrames = 0;
    qext_refresh_titles();
    reload_rows();

    s_live = true;
    return 0;
}

int loop(void)
{
    if (!s_live)
        return -1;

    handle_input();
    s_frame++;
    if (s_frame >= 300) {
        s_frame = 0;
        qext_refresh_titles();
        reload_rows();
    }
    draw_frame(s_launchBlack);
    submit_frame();
    return 0;
}

void onShutdown(void)
{
    if (!s_live)
        return;
    s_live = false;
    s_padReady = false;
    dkQueueWaitIdle(s_queue);
    dkQueueDestroy(s_queue);
    dkCmdBufDestroy(s_cmdbuf);
    if (s_fontOk) {
        dkMemBlockDestroy(s_descMem);
        dkMemBlockDestroy(s_fontMem);
        s_fontOk = false;
    }
    dkMemBlockDestroy(s_vtxMem);
    s_vtxCpu = NULL;
    s_txtCpu = NULL;
    dkMemBlockDestroy(s_cmdMem);
    dkMemBlockDestroy(s_codeMem);
    dkSwapchainDestroy(s_swapchain);
    dkMemBlockDestroy(s_fbMem);
    dkDeviceDestroy(s_device);
}

void onReload(void)
{
    if (!s_live)
        return;
    s_launchBlack = false;
    font_refresh();
    qext_refresh_titles();
    reload_rows();
}

void onPowerButton(void)
{
}

void onOpen(void)
{
    if (!s_live)
        return;
    s_launchBlack = false;
    font_refresh();
    qext_refresh_titles();
    reload_rows();
}

void onHomeButton(void)
{
    if (!s_live)
        return;
    s_sel = 0;
}



