/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include <cstring>
#include <malloc.h>
#include <switch.h>
#include <dlink/dlink.h>
#include <shared/logging.hpp>
#include "app.hpp"
#include "la.hpp"
#include "sys.hpp"
#include "titles.hpp"

extern "C" {
    /* Otherwise nothing will work*/
    u32 __nx_applet_type = AppletType_SystemApplet;
    u32 __nx_fs_num_sessions = 3;
    /* TODO: find out why I dont need NvServiceType_System */
    NvServiceType __nx_nv_service_type = NvServiceType_Applet;

    /* For proper GFX a high transfermem size is needed */
    u32 __nx_nv_transfermem_size = 0x800000;

    /* Time */
    TimeServiceType __nx_time_service_type = TimeServiceType_System;
}

static constexpr size_t kHeapFloor = 32u * 1024u * 1024u; /* 32MB */
static constexpr size_t kHeapCap = 96u * 1024u * 1024u; /* 96MB */
static size_t g_heapSize = 0;
extern char *fake_heap_start;
extern char *fake_heap_end;

/* Initialize the heap properly, idk if this is needed */
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

#define FB_W 1280
#define FB_H 720

/* Global layers/windows */
static ViDisplay g_display{};
static ViLayer g_layer{};
static NWindow g_win{};

/* Statuses */
static bool g_vi = false;
static bool g_layerLive = false;
static bool g_winLive = false;

/* Pointers to dynamic libraries */
static void *g_neko;
static void *g_menu;

/* Pointers to menu callbacks */
static int (*s_onBoot)(NWindow *);
static void (*s_onReload)(void);
static void (*s_onShutdown)(void);
static void (*s_onPowerButton)(void);
static void (*s_onOpen)(void);
static int (*s_loop)(void);
static void (*s_onHomeButton)(void);
static bool g_booted;

static void gfx_exit(void)
{
    /* Exit all GFX stuff */
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

static Result gfx_layer_init(void)
{
    /* Create VI Layers for our service */
    Result rc = 0;
    if (!g_layerLive) {
        /* Create the layer */
        rc = viCreateLayer(&g_display, &g_layer);
        if (R_FAILED(rc))
            return rc;
        g_layerLive = true;

        /* Fit the layer to the display. TODO: add 480/1080p support */
        viSetLayerSize(&g_layer, FB_W, FB_H);

        /* Use a low priority to avoid overwriting other system UI */
        viSetLayerZ(&g_layer, 1);

        /* Default scaling mode */
        viSetLayerScalingMode(&g_layer, ViScalingMode_FitToLayer);

        /* Actually show the layer*/
        viSetContentVisibility(true);
        
        /* If the alpha is the default it won't display our layer correctly */
        viSetDisplayAlpha(&g_display, 1.0f);
    }
    if (!g_winLive) {
        rc = nwindowCreateFromLayer(&g_win, &g_layer);
        if (R_FAILED(rc))
            return rc;
        g_winLive = true;
        eventClose(&g_win.event);
    }
    return rc;
}

struct HostDyn {
    int64_t d_tag;
    uint64_t d_val;
};
struct HostSym {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};
extern HostDyn _DYNAMIC[];

static uint64_t dyn_fix(uint64_t base, uint64_t v)
{
    return v < base ? base + v : v;
}

static Result host_provide_all(void)
{
    /* Do some memory magic to resolve all symbols */
    uint8_t *probe = (uint8_t *)&host_provide_all;
    MemoryInfo mi{};
    uint32_t pi = 0;
    Result rc = svcQueryMemory(&mi, &pi, (uint64_t)probe);

    if (R_FAILED(rc))
        return rc;

    uint64_t base = mi.addr;
    uint32_t *hash = nullptr;
    const char *strtab = nullptr;
    HostSym *symtab = nullptr;

    for (HostDyn *d = _DYNAMIC; d->d_tag != 0; d++) {
        if (d->d_tag == 4)
            hash = (uint32_t *)dyn_fix(base, d->d_val);
        else if (d->d_tag == 5)
            strtab = (const char *)dyn_fix(base, d->d_val);
        else if (d->d_tag == 6)
            symtab = (HostSym *)dyn_fix(base, d->d_val);
    }

    if (!hash || !strtab || !symtab)
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    
    uint32_t nchain = hash[1];
    uint32_t count = 0;

    /* Iterate over functions and their names to provide them */
    for (uint32_t i = 1; i < nchain; i++) {
        HostSym *s = &symtab[i];
        uint8_t bind = (uint8_t)(s->st_info >> 4);
        uint8_t type = (uint8_t)(s->st_info & 0xF);

        if (s->st_shndx == 0 || (bind != 1 && bind != 2))
            continue;

        if (type != 1 && type != 2)
            continue;

        const char *name = strtab + s->st_name;

        if (name[0] == 0)
            continue;
    
        if (R_SUCCEEDED(dlink_provide(name, (void *)dyn_fix(base, s->st_value))))
            count++;
    }

    logging::LogLine("[qlaunch-ext] provided %u functions to libraries", count);
    return 0;
}

static void menu_unload(void)
{
    /* Clear all pointers */
    s_onBoot = nullptr;
    s_onReload = nullptr;
    s_onShutdown = nullptr;
    s_onPowerButton = nullptr;
    s_onOpen = nullptr;
    s_loop = nullptr;
    s_onHomeButton = nullptr;

    /* Close dynamic libs */
    if (g_menu) {
        dlclose(g_menu);
        g_menu = nullptr;
    }
    if (g_neko) {
        dlclose(g_neko);
        g_neko = nullptr;
    }
}

/* One boot stage per part of the initcode, so we can keep applet services silent */
enum BootStage {
    BootStage_Provide,
    BootStage_Layer,
    BootStage_Neko,
    BootStage_Menu,
    BootStage_Syms,
    BootStage_OnBoot,
    BootStage_Done,
};
static int g_bootStage;

static void wait_wake(uint64_t timeout_ns)
{
    Event *ev = appletGetMessageEvent();
    if (ev == nullptr) {
        svcSleepThread(timeout_ns);
        return;
    }
    s32 idx = -1;
    waitMulti(&idx, timeout_ns, waiterForEvent(ev));
}

static Result boot_fail(Result rc)
{
    menu_unload();
    g_bootStage = BootStage_Provide;
    return rc;
}

static Result boot_step(void)
{
    Result rc = 0;
    switch (g_bootStage) {
    case BootStage_Provide:
        logging::LogLine("[qlaunch-ext] trying to boot menu...");
        rc = host_provide_all();
        logging::LogLine("[qlaunch-ext] functions loaded (rc=0x%X)", rc);
        if (R_FAILED(rc))
            return boot_fail(rc);
        g_bootStage = BootStage_Layer;
        break;
    case BootStage_Layer:
        rc = gfx_layer_init();
        logging::LogLine("[qlaunch-ext] initialized layers (rc=0x%X)", rc);
        if (R_FAILED(rc))
            return boot_fail(rc);
        g_bootStage = BootStage_Neko;
        break;
    case BootStage_Neko:
        dlink_set_library_dir(NULL);
        if (!g_neko) {
            g_neko = dlopen("neko3d", RTLD_GLOBAL);
            logging::LogLine("[qlaunch-ext] opening neko3d at %p (dlerr=%s)", g_neko,
                             g_neko ? "NONE" : dlerror());
            if (!g_neko)
                return boot_fail(MAKERESULT(Module_Libnx, LibnxError_NotFound));
        }
        g_bootStage = BootStage_Menu;
        break;
    case BootStage_Menu:
        if (g_menu) {
            dlclose(g_menu);
            g_menu = nullptr;
        }
        g_menu = dlopen("sdmc:/switch/qlaunch-ext/menus/menu.dnro", RTLD_LOCAL);
        logging::LogLine("[qlaunch-ext] opening menu at %p (dlerr=%s)", g_menu,
                         g_menu ? "NONE" : dlerror());
        if (!g_menu)
            return boot_fail(MAKERESULT(Module_Libnx, LibnxError_NotFound));
        g_bootStage = BootStage_Syms;
        break;
    case BootStage_Syms:
        s_onBoot = (int (*)(NWindow *))dlsym(g_menu, "onBoot");
        s_onReload = (void (*)(void))dlsym(g_menu, "onReload");
        s_onShutdown = (void (*)(void))dlsym(g_menu, "onShutdown");
        s_onPowerButton = (void (*)(void))dlsym(g_menu, "onPowerButton");
        s_onOpen = (void (*)(void))dlsym(g_menu, "onOpen");
        s_loop = (int (*)(void))dlsym(g_menu, "loop");
        s_onHomeButton = (void (*)(void))dlsym(g_menu, "onHomeButton");
        if (!s_onBoot || !s_onReload || !s_onShutdown || !s_onPowerButton || !s_onOpen || !s_loop ||
            !s_onHomeButton) {
            logging::LogLine("[qlaunch-ext] can't load menu symbols");
            return boot_fail(MAKERESULT(Module_Libnx, LibnxError_NotFound));
        }
        g_bootStage = BootStage_OnBoot;
        break;
    case BootStage_OnBoot: {
        int brc = s_onBoot(&g_win);
        logging::LogLine("[qlaunch-ext] ran onBoot (rc=%d)", brc);
        if (brc != 0)
            return boot_fail(MAKERESULT(Module_Libnx, LibnxError_BadGfxInit));
        g_bootStage = BootStage_Done;
        logging::LogLine("[qlaunch-ext] started menu");
        break;
    }
    default:
        break;
    }
    return 0;
}

extern "C" void __appInit(void)
{
    /* Initialize services */
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
    if (R_FAILED(vrc))
        vrc = viInitialize(ViServiceType_System);
    if (R_SUCCEEDED(vrc)) {
        vrc = viOpenDefaultDisplay(&g_display);
        if (R_SUCCEEDED(vrc))
            g_vi = true;
    }
    /* Initialize logging */
    logging::Initialize();
    if (R_SUCCEEDED(mnt)) {
        logging::SetLogOutput(logging::LogOutput_File);
        logging::SetFileLoggingPath("sdmc:/qlaunch-ext-log.txt");
    } else {
        /* Fallback to UART if SD can't be found*/
        logging::SetLogOutput(logging::LogOutput_UART);
    }

    logging::LogLine("[qlaunch-ext] initialized");
}

extern "C" void __appExit(void)
{
    /* Exit logger */
    logging::LogLine("[qlaunch-ext] exiting");
    logging::Exit();
    
    /* Exit services */
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

static bool g_displayApproved = false;
static bool g_homePending = false;
static u64 g_homePendingAt = 0;
static bool g_overlayShown = false;
static bool g_menuRefreshPending = false;

static u64 now_ms(void)
{
    return svcGetSystemTick() / 19200ULL;
}

static void reopen_menu(const char *source)
{
    titles::Refresh();
    if (g_booted && s_onOpen)
        s_onOpen();
    g_homePending = false;
    logging::LogLine("[qlaunch-ext] reopening from %s t=%llu", source,
                     (unsigned long long)now_ms());
}

static void do_home(const char *source)
{
    logging::LogLine("[qlaunch-ext] Home request (%s) t=%llu", source,
                     (unsigned long long)now_ms());
    if (la::IsActive()) {
        la::Terminate();
        reopen_menu(source);
        return;
    }
    if (app::IsActive() && app::HasForeground()) {
        Result rc = sys::SetForeground();
        logging::LogLine("[qlaunch-ext] fg request rc=0x%X", rc);
        if (R_SUCCEEDED(rc)) {
            g_homePending = true;
            g_homePendingAt = now_ms();
        }
        return;
    }
    if (g_booted && s_onHomeButton)
        s_onHomeButton();
}

static void do_sleep(const char *source)
{
    logging::LogLine("[qlaunch-ext] sleep request (%s) t=%llu", source,
                     (unsigned long long)now_ms());
    sys::EnterSleep();
}

static void pump_general_channel(void)
{
    struct SamsHdr {
        u32 magic;
        u32 ver;
        u32 msg;
        u32 rsv;
    };
    for (int i = 0; i < 16; i++) {
        AppletStorage st{};
        if (R_FAILED(appletPopFromGeneralChannel(&st)))
            return;
        SamsHdr h{};
        s64 sz = 0;
        appletStorageGetSize(&st, &sz);
        if (sz >= (s64)sizeof(h))
            appletStorageRead(&st, 0, &h, sizeof(h));
        appletStorageClose(&st);
        if (h.magic != 0x534D4153) {
            logging::LogLine("[qlaunch-ext] sams invalid magic=0x%X sz=%lld", h.magic,
                             (long long)sz);
            continue;
        }
        logging::LogLine("[qlaunch-ext] sams msg=%u t=%llu", h.msg,
                         (unsigned long long)now_ms());
        switch (h.msg) {
        case 2:
            do_home("sams");
            break;
        case 3:
            do_sleep("sams");
            break;
        case 5:
            appletStartShutdownSequence();
            break;
        case 6:
            appletStartRebootSequence();
            break;
        case 16:
            g_overlayShown = true;
            break;
        case 17:
            g_overlayShown = false;
            if (g_booted)
                g_menuRefreshPending = true;
            break;
        default:
            break;
        }
    }
}

static void pump_applet_messages(void)
{
    for (int i = 0; i < 32; i++) {
        u32 msg = 0;
        if (R_FAILED(appletGetMessage(&msg)))
            return;
        logging::LogLine("[qlaunch-ext] ae msg=%u t=%llu", msg,
                         (unsigned long long)now_ms());
        switch (msg) {
        case 1: /* ChangeIntoForeground */
            if (!g_displayApproved) {
                appletApproveToDisplay();
                g_displayApproved = true;
            }
            if (g_homePending)
                reopen_menu("fg");
            break;
        case 2: /* ChangeIntoBackground */
            break;
        case 6: /* ApplicationExited */
            titles::Refresh();
            if (g_booted)
                g_menuRefreshPending = true;
            break;
        case 15: /* FocusStateChanged */
            break;
        case 20: /* DetectShortPressingHomeButton */
            do_home("ae");
            break;
        case 22: /* DetectShortPressingPowerButton */
        case 29: /* AutoPowerDown */
        case 32: /* DetectReceivingCecSystemStandby */
            do_sleep("ae");
            break;
        case 26: /* FinishedSleepSequence (wakeup) */
            appletRequestToGetForeground();
            if (g_booted)
                g_menuRefreshPending = true;
            break;
        case 35: /* RequestToDisplay */
            if (!g_displayApproved) {
                appletApproveToDisplay();
                g_displayApproved = true;
            }
            break;
        default:
            break;
        }
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    logging::LogLine("[qlaunch-ext] initialized (build %s %s)", __DATE__, __TIME__);
    {
        u32 hv = hosversionGet();
        logging::LogLine("[qlaunch-ext] fw %u.%u.%u", HOSVER_MAJOR(hv), HOSVER_MINOR(hv), HOSVER_MICRO(hv));
    }
    {
        Result iprc = appletLoadAndApplyIdlePolicySettings();
        logging::LogLine("[qlaunch-ext] idle policy rc=0x%X", iprc);
    }
    /* Drain anything queued during boot. */
    pump_general_channel();
    pump_applet_messages();
    static bool was_active = false;
    static bool was_overlay = false;
    static u64 last_hb = 0;

    while (true) {
        pump_general_channel();
        pump_applet_messages();
        /* game exit */
        bool active = app::IsActive();
        if (was_active && !active) {
            logging::LogLine("[qlaunch-ext] game exited t=%llu",
                             (unsigned long long)now_ms());
            titles::Refresh();
            if (g_booted)
                g_menuRefreshPending = true;
        }
        was_active = active;
        bool overlay_now = la::IsActive() || g_overlayShown;
        if (was_overlay && !overlay_now) {
            logging::LogLine("[qlaunch-ext] applet exited t=%llu",
                             (unsigned long long)now_ms());
            if (g_booted)
                g_menuRefreshPending = true;
        }
        was_overlay = overlay_now;

        if (g_homePending && now_ms() - g_homePendingAt > 2000) {
            logging::LogLine("[qlaunch-ext] home pending timeout, reopening");
            reopen_menu("timeout");
        }

        /* Stop rendering on a app */
        bool want_menu = !(app::IsActive() && app::HasForeground()) && !overlay_now;
        if (!want_menu) {
            u64 now = now_ms();
            if (now - last_hb > 5000) {
                last_hb = now;
                logging::LogLine("[qlaunch-ext] hb: app=%d fg=%d ov=%d t=%llu",
                                 app::IsActive() ? 1 : 0,
                                 app::HasForeground() ? 1 : 0,
                                 overlay_now ? 1 : 0,
                                 (unsigned long long)now);
            }
            svcSleepThread(16000000ULL);
            continue;
        }
        if (g_menuRefreshPending && g_booted) {
            g_menuRefreshPending = false;
            if (s_onOpen)
                s_onOpen();
        }
        if (!g_booted) {
            Result brc = boot_step();
            if (R_FAILED(brc)) {
                logging::LogLine("[qlaunch-ext] boot stage failed (rc=0x%X)", brc);
                wait_wake(500000000ULL);
                continue;
            }
            if (g_bootStage == BootStage_Done) {
                g_booted = true;
                logging::LogLine("[qlaunch-ext] booted menu");
            } else {
                wait_wake(16000000ULL);
                continue;
            }
        }
        int lrc = s_loop();
        if (lrc != 0) {
            logging::LogLine("[qlaunch-ext] exited menu loop (rc=0x%d)", lrc);
            break;
        }
    }

    if (g_booted) {
        if (s_onShutdown)
            s_onShutdown();
        menu_unload();
    }
    gfx_exit();
    /* Stop our heap */
    heap_set(kHeapFloor);
    return 0;
}
