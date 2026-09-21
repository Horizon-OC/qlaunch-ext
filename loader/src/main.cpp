/* qlaunch-ext (C) 2026 Souldbminer */
/* Pain... And suffering. */

#include <cstring>
#include <malloc.h>
#include <shared/logging.hpp>
#include <switch.h>

extern "C" {
u32 __nx_applet_type = AppletType_SystemApplet;
u32 __nx_fs_num_sessions = 3;
NvServiceType __nx_nv_service_type = NvServiceType_System;
u32 __nx_nv_transfermem_size = 0x800000;
TimeServiceType __nx_time_service_type = TimeServiceType_System;
}

static constexpr size_t kHeapFloor = 32u * 1024u * 1024u;
static constexpr size_t kHeapCap = 96u * 1024u * 1024u;
static size_t g_heapSize = 0;
extern char *fake_heap_start;
extern char *fake_heap_end;

extern "C" void __libnx_initheap(void)
{
    void *addr = nullptr;
    if (R_SUCCEEDED(svcSetHeapSize(&addr, kHeapFloor))) {
        g_heapSize = kHeapFloor;
        fake_heap_start = (char *)addr;
        fake_heap_end = (char *)addr + g_heapSize;
    } else {
        g_heapSize = 0;
        fake_heap_start = nullptr;
        fake_heap_end = nullptr;
    }
}

static Result heap_set(size_t target)
{
    if (target < kHeapFloor)
        target = kHeapFloor;
    if (target > kHeapCap)
        target = kHeapCap;
    if (target == g_heapSize)
        return 0;
    void *addr = nullptr;
    Result rc = svcSetHeapSize(&addr, target);
    if (R_FAILED(rc))
        return rc;
    if (g_heapSize != 0 && addr != (void *)fake_heap_start)
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    g_heapSize = target;
    fake_heap_start = (char *)addr;
    fake_heap_end = (char *)addr + g_heapSize;
    return 0;
}

/* Add docked later? */
#define FB_W 1280
#define FB_H 720

static ViDisplay g_display{};
static ViLayer g_layer{};
static NWindow g_win{};
static Framebuffer g_fb{};
static bool g_vi = false;
static bool g_layerLive = false;
static bool g_winLive = false;
static bool g_fbLive = false;

static void gfx_exit(void)
{
    if (g_fbLive) {
        framebufferClose(&g_fb);
        g_fbLive = false;
    }
    nvMapExit();
    nvFenceExit();
    nvGpuExit();
    nvExit();
    if (g_winLive) {
        nwindowClose(&g_win);
        g_winLive = false;
    }
    if (g_layerLive) {
        viCloseLayer(&g_layer);
        g_layerLive = false;
    }
    if (g_vi) {
        viCloseDisplay(&g_display);
        g_vi = false;
    }
}

static Result gfx_fb_create(void)
{
    const u32 width = FB_W;
    const u32 height = FB_H;
    const u32 format = PIXEL_FORMAT_RGB_565;
    const int num_fbs = 1;
    const u32 bpp = 2;
    const u32 block_h = 128;
    u32 width_aligned = (width + 63) & ~63u;
    u32 height_aligned = (height + block_h - 1) & ~(block_h - 1);
    u32 width_aligned_bytes = width_aligned * bpp;
    u32 stride = width_aligned_bytes;
    u32 fb_size = stride * height_aligned;
    u32 total_size = fb_size * (u32)num_fbs;
    size_t buf_size = (total_size + 0xFFF) & ~(size_t)0xFFF;
    Result rc;

    logging::LogLine("[qlaunch-ext] gfx: setdim %ux%u", width, height);
    rc = nwindowSetDimensions(&g_win, width, height);
    logging::LogLine("[qlaunch-ext] gfx: setdim rc=0x%X", rc);
    if (R_FAILED(rc))
        return rc;

    logging::LogLine("[qlaunch-ext] gfx: nvInitialize");
    rc = nvInitialize();
    logging::LogLine("[qlaunch-ext] gfx: nvInitialize rc=0x%X", rc);
    if (R_FAILED(rc))
        return rc;

    logging::LogLine("[qlaunch-ext] gfx: nvMapInit");
    rc = nvMapInit();
    logging::LogLine("[qlaunch-ext] gfx: nvMapInit rc=0x%X", rc);
    if (R_FAILED(rc)) {
        nvExit();
        return rc;
    }

    logging::LogLine("[qlaunch-ext] gfx: nvFenceInit");
    rc = nvFenceInit();
    logging::LogLine("[qlaunch-ext] gfx: nvFenceInit rc=0x%X", rc);
    if (R_FAILED(rc)) {
        nvMapExit();
        nvExit();
        return rc;
    }

    logging::LogLine("[qlaunch-ext] gfx: alloc %u", (unsigned)buf_size);
    void *buf = memalign(0x1000, buf_size);
    logging::LogLine("[qlaunch-ext] gfx: alloc -> %p", buf);
    if (buf == NULL) {
        nvFenceExit();
        nvMapExit();
        nvExit();
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    memset(buf, 0, buf_size);

    logging::LogLine("[qlaunch-ext] gfx: nvMapCreate");
    rc = nvMapCreate(&g_fb.map, buf, buf_size, 0x20000, NvKind_Pitch, true);
    logging::LogLine("[qlaunch-ext] gfx: nvMapCreate rc=0x%X", rc);
    if (R_FAILED(rc)) {
        free(buf);
        nvFenceExit();
        nvMapExit();
        nvExit();
        return rc;
    }

    NvGraphicBuffer grbuf;
    memset(&grbuf, 0, sizeof(grbuf));
    grbuf.header.num_ints = (sizeof(NvGraphicBuffer) - sizeof(NativeHandle)) / 4;
    grbuf.unk0 = -1;
    grbuf.nvmap_id = (s32)nvMapGetId(&g_fb.map);
    grbuf.magic = 0xDAFFCAFF;
    grbuf.pid = 42;
    grbuf.usage = GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
    grbuf.format = format;
    grbuf.ext_format = format;
    grbuf.stride = width_aligned;
    grbuf.total_size = fb_size;
    grbuf.num_planes = 1;
    grbuf.planes[0].width = width;
    grbuf.planes[0].height = height;
    grbuf.planes[0].color_format = NvColorFormat_R5G6B5;
    grbuf.planes[0].layout = NvLayout_BlockLinear;
    grbuf.planes[0].kind = NvKind_Generic_16BX2;
    grbuf.planes[0].block_height_log2 = 4;
    grbuf.planes[0].pitch = width_aligned_bytes;
    grbuf.planes[0].size = fb_size;
    grbuf.planes[0].offset = 0;

    for (int i = 0; i < num_fbs; i++) {
        grbuf.planes[0].offset = (u32)(i * fb_size);
        logging::LogLine("[qlaunch-ext] gfx: configureBuffer %d off=%u", i,
                         (unsigned)grbuf.planes[0].offset);
        rc = nwindowConfigureBuffer(&g_win, i, &grbuf);
        logging::LogLine("[qlaunch-ext] gfx: configureBuffer rc=0x%X", rc);
        if (R_FAILED(rc)) {
            nvMapClose(&g_fb.map);
            free(buf);
            nvFenceExit();
            nvMapExit();
            nvExit();
            return rc;
        }
    }

    g_fb.win = &g_win;
    g_fb.buf = buf;
    g_fb.buf_linear = NULL;
    g_fb.stride = stride;
    g_fb.width_aligned = width_aligned;
    g_fb.height_aligned = height_aligned;
    g_fb.num_fbs = num_fbs;
    g_fb.fb_size = fb_size;
    g_fb.has_init = true;

    size_t linear_size = (size_t)stride * ((height + 7) & ~7u);
    logging::LogLine("[qlaunch-ext] gfx: linear alloc %u", (unsigned)linear_size);
    g_fb.buf_linear = memalign(0x1000, linear_size);
    logging::LogLine("[qlaunch-ext] gfx: linear -> %p", g_fb.buf_linear);
    if (g_fb.buf_linear == NULL) {
        nvMapClose(&g_fb.map);
        free(buf);
        memset(&g_fb, 0, sizeof(g_fb));
        nvFenceExit();
        nvMapExit();
        nvExit();
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    memset(g_fb.buf_linear, 0, linear_size);
    g_fbLive = true;
    return 0;
}

static Result gfx_init(void)
{
    Result rc = 0;
    if (!g_layerLive) {
        logging::LogLine("[qlaunch-ext] gfx: create layer");
        rc = viCreateLayer(&g_display, &g_layer);
        logging::LogLine("[qlaunch-ext] gfx: layer rc=0x%X", rc);
        if (R_FAILED(rc))
            return rc;
        g_layerLive = true;
        viSetLayerSize(&g_layer, FB_W, FB_H);
        s32 zmax = 10;
        Result zrc = viGetZOrderCountMax(&g_display, &zmax);
        logging::LogLine("[qlaunch-ext] gfx: zmax=%d rc=0x%X", zmax, zrc);
        if (R_FAILED(zrc))
            zmax = 10;
        viSetLayerZ(&g_layer, 1);
        viSetLayerScalingMode(&g_layer, ViScalingMode_FitToLayer);
        viSetContentVisibility(true);
        logging::LogLine("[qlaunch-ext] gfx: display alpha");
        Result arc = viSetDisplayAlpha(&g_display, 1.0f);
        logging::LogLine("[qlaunch-ext] gfx: display alpha rc=0x%X", arc);
        s32 dw = 0, dh = 0;
        Result drc = viGetDisplayResolution(&g_display, &dw, &dh);
        logging::LogLine("[qlaunch-ext] gfx: display %dx%d rc=0x%X", dw, dh, drc);
    }
    if (!g_winLive) {
        rc = nwindowCreateFromLayer(&g_win, &g_layer);
        logging::LogLine("[qlaunch-ext] gfx: nwindow rc=0x%X", rc);
        if (R_FAILED(rc))
            return rc;
        g_winLive = true;

        eventClose(&g_win.event);
    }
    rc = gfx_fb_create();
    return rc;
}

static void draw_red(void)
{
    u32 stride = 0;
    u16 *px = (u16 *)framebufferBegin(&g_fb, &stride);
    if (px == nullptr)
        return;
    u32 stride_px = stride / sizeof(u16);
    const u16 red = (u16)((31 << 11) | (0 << 5) | 0);
    for (u32 y = 0; y < FB_H; y++) {
        for (u32 x = 0; x < FB_W; x++)
            px[y * stride_px + x] = red;
    }
    framebufferEnd(&g_fb);
}
extern "C" void __appInit(void)
{
    smInitialize();
    fsInitialize();
    appletInitialize();
    timeInitialize();
    setsysInitialize();
    setInitialize();
    SetSysFirmwareVersion fw{};
    if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw)))
        hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro) | BIT(31));
    nsInitialize();
    ldrShellInitialize();
    accountInitialize(AccountServiceType_System);
    nssuInitialize();
    avmInitialize();
    psmInitialize();
    lblInitialize();
    hidInitialize();
    Result mnt = fsdevMountSdmc();

    Result vrc = viInitialize(ViServiceType_Manager);
    bool vi_manager = R_SUCCEEDED(vrc);
    if (R_FAILED(vrc))
        vrc = viInitialize(ViServiceType_System);
    if (R_SUCCEEDED(vrc)) {
        vrc = viOpenDefaultDisplay(&g_display);
        if (R_SUCCEEDED(vrc))
            g_vi = true;
    }
    logging::Initialize();
    if (g_heapSize == 0)
        svcOutputDebugString("[qlaunch-ext] heap init FAILED", 32);
    if (R_SUCCEEDED(mnt)) {
        logging::SetLogOutput(logging::LogOutput_File);
        logging::SetFileLoggingPath("sdmc:/qlaunch-ext-log.txt");
    } else {
        logging::SetLogOutput(logging::LogOutput_UART);
    }
    logging::LogLine("[qlaunch-ext] init heap=%u vi=%d mgr=%d vrc=0x%X mnt=0x%X",
                     (unsigned)g_heapSize, g_vi ? 1 : 0, vi_manager ? 1 : 0, vrc, mnt);
}
extern "C" void __appExit(void)
{
    logging::LogLine("[qlaunch-ext] exit heap=%u", (unsigned)g_heapSize);
    logging::Exit();
    gfx_exit();
    viExit();
    hidExit();
    lblExit();
    psmExit();
    avmExit();
    nssuExit();
    accountExit();
    ldrShellExit();
    nsExit();
    setExit();
    setsysExit();
    timeExit();
    appletExit();
    fsdevUnmountAll();
    fsExit();
    smExit();
}
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    logging::LogLine("[qlaunch-ext] main entry");
    u64 frame = 0;
    bool approved = false;
    bool announced = false;
    while (appletMainLoop()) {
        u32 msg = 0;
        while (R_SUCCEEDED(appletGetMessage(&msg))) {
            logging::LogLine("[qlaunch-ext] applet msg=%u", msg);
            if (msg == (u32)AppletMessage_RequestToDisplay && !approved) {
                appletApproveToDisplay();
                approved = true;
            }
        }
        if (!g_fbLive) {
            if ((frame % 120) == 0) {
                Result r = gfx_init();
                logging::LogLine("[qlaunch-ext] gfx retry rc=0x%X heap=%u", r,
                                 (unsigned)g_heapSize);
            }
            frame++;
            svcSleepThread(16e6);
            continue;
        }
        if (!announced) {
            u64 aruid = appletGetAppletResourceUserId();
            u32 ww = 0, hh = 0;
            nwindowGetDimensions(&g_win, &ww, &hh);
            logging::LogLine("[qlaunch-ext] first draw aruid=%llu windim=%ux%u focus=%d",
                             aruid, ww, hh, (int)appletGetFocusState());
            u32 ds = 0;
            logging::LogLine("[qlaunch-ext] first dequeue");
            u16 *dpx = (u16 *)framebufferBegin(&g_fb, &ds);
            logging::LogLine("[qlaunch-ext] dequeued px=%p stride=%u", dpx, ds);
            if (dpx != nullptr) {
                u32 dsp = ds / sizeof(u16);
                const u16 red = (u16)((31 << 11) | (0 << 5) | 0);
                for (u32 y = 0; y < FB_H; y++) {
                    for (u32 x = 0; x < FB_W; x++)
                        dpx[y * dsp + x] = red;
                }
            }
            logging::LogLine("[qlaunch-ext] first queue");
            framebufferEnd(&g_fb);
            volatile const u16 *vb = (volatile const u16 *)g_fb.buf;
            u32 sum = 0;
            for (u32 i = 0; i < 64; i++)
                sum += vb[i];
            logging::LogLine("[qlaunch-ext] queued bufsum=0x%X", sum);
            logging::LogLine("[qlaunch-ext] red screen loop heap=%u", (unsigned)g_heapSize);
            announced = true;
        }
        AppletFocusState st = appletGetFocusState();
        if ((frame % 120) == 0)
            logging::LogLine("[qlaunch-ext] frame=%llu focus=%d", frame, (int)st);
        frame++;
        draw_red();
    }
    gfx_exit();
    heap_set(kHeapFloor);
    return 0;
}
