/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include <cstdio>
#include <cstring>
#include <malloc.h>
#include <unistd.h>
#include <switch.h>
#include <switch/display/parcel.h>
#include <dlink/dlink.h>
#include <shared/logging.hpp>
#include "app.hpp"
#include "la.hpp"
#include "messages.hpp"
#include "plugins.hpp"
#include "sys.hpp"
#include "titles.hpp"
#include "album.hpp"
#include "arena.hpp"

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

/* NOTE: If you use more than 60mb then the browser will fail to load and fatal the console */
/*       The heap must be 2MB aligned, otherwise it will cause issues and fails to resize   */
static constexpr size_t kHeapFloor = 60u * 1024u * 1024u; /* 60MB (AppletRequired) */
static constexpr size_t kHeapCap = 384u * 1024u * 1024u; /* 384MB (High) */
static constexpr size_t kHeapBurst = 440u * 1024u * 1024u; /* 440MB (Burst) */
static constexpr size_t kHeapSmall = 1u * 1024 * 1024; /* malloc fallback/small heap */
static constexpr size_t kBigAlloc = 0; /* 0 = route all malloc to arena */
static size_t g_heapSize = 0;
extern char *fake_heap_start;
extern char *fake_heap_end;

static Mutex s_arenaLock;
static char *s_arenaStart = nullptr;
static char *s_arenaEnd = nullptr;
static bool s_arenaReady = false;

static inline bool InArena(void *p)
{
    return s_arenaStart && (char *)p >= s_arenaStart && (char *)p < s_arenaEnd;
}

static Result heap_set_total(size_t target);

static void *ArenaAlloc(size_t n, size_t align)
{
    if (n < kBigAlloc || !s_arenaReady || s_arenaEnd == s_arenaStart)
        return nullptr;
    mutexLock(&s_arenaLock);
    void *p = arena::Alloc(n, align);
    mutexUnlock(&s_arenaLock);
    if (p)
        return p;

    /* No contiguous hole satisfied the request. Grow the kernel heap
       (up to Burst) and retry, so fragmentation or a trimmed heap turns
       into growth instead of allocation failure: SDL audio queue chunks
       and lazily decoded WAVs must not observe NULL when the heap has
       reserved space left. heap_set_total does its own locking. */
    size_t toAdd = n + align + arena::kMinBlock + (32u << 20);
    size_t target = g_heapSize + toAdd;
    if (target < g_heapSize || target > kHeapBurst)
        target = kHeapBurst;
    if (target <= g_heapSize)
        return nullptr;
    if (R_FAILED(heap_set_total(target)))
        return nullptr;
    mutexLock(&s_arenaLock);
    p = arena::Alloc(n, align);
    mutexUnlock(&s_arenaLock);
    return p; /* still null: hard OOM, caller falls back on malloc's heap */
}

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

static unsigned heap_release_arena(FILE *also);

/* Grow or establish heap to `target` (bytes). Splits into small malloc heap
   + arena on first arena setup; extends arena on grow. Respects out_addr. */
static Result heap_set_total(size_t target)
{
    if (target < kHeapFloor)
        target = kHeapFloor;
    if (target > kHeapBurst)
        target = kHeapBurst;

    /* Shrink requests: release arena from the top first. */
    if (s_arenaReady && target < g_heapSize) {
        heap_release_arena(nullptr);
        if (g_heapSize <= target)
            return 0; /* release trimmed as far as possible (may be pinned above target) */
        /* try to shrink further only if arena is clear down to target */
        mutexLock(&s_arenaLock);
        char *newEnd = fake_heap_start + target;
        if (newEnd < s_arenaStart)
            newEnd = s_arenaStart;
        if (newEnd < s_arenaEnd) {
            /* can only shrink to newEnd if no live blocks above it; TrimEnd/scan
               already done by release - if s_arenaEnd > newEnd, we're pinned */
            mutexUnlock(&s_arenaLock);
            return 0; /* pinned by live arena allocations; caller tolerates RC/failure semantics */
        }
        mutexUnlock(&s_arenaLock);
    }

    if (target == g_heapSize && s_arenaReady)
        return 0;

    void *addr = nullptr;
    Result rc = svcSetHeapSize(&addr, target);
    if (R_FAILED(rc))
        return rc;
    if (g_heapSize != 0 && addr != (void *)fake_heap_start)
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);

    fake_heap_start = (char *)addr;

    mutexLock(&s_arenaLock);
    if (!s_arenaReady) {
        size_t small = kHeapSmall;
        char *brk = (char *)sbrk(0);
        size_t used = (brk > fake_heap_start) ? ((size_t)(brk - fake_heap_start) + 0x1FFFFFu) & ~(size_t)0x1FFFFFu : 0;
        if (small < used)
            small = used;
        if (small > target)
            small = target;
        s_arenaStart = fake_heap_start + small;
        s_arenaEnd = s_arenaStart;
        fake_heap_end = s_arenaStart; /* confine malloc heap */
        arena::g_free = nullptr;
        s_arenaReady = true;
    }
    char *end = fake_heap_start + target;
    if (end > s_arenaEnd) {
        arena::Insert(s_arenaEnd, (size_t)(end - s_arenaEnd));
        s_arenaEnd = end;
    } else if (end < s_arenaEnd) {
        /* The kernel shrank the heap below the previous arena end.
           Drop free blocks at/above the new end so the free list can
           never hand out pointers to unmapped memory. */
        arena::PruneAbove(end);
        s_arenaEnd = end;
        fake_heap_end = s_arenaStart;
    }
    g_heapSize = target;
    mutexUnlock(&s_arenaLock);
    return 0;
}

/* Release arena space from the top (down to highest live block) and shrink
   heap. Returns resulting heap size in MB (0 on failure to query meaningfully). */
static unsigned heap_release_arena(FILE *also)
{
    if (!s_arenaReady)
        return (unsigned)(g_heapSize >> 20);

    mutexLock(&s_arenaLock);
    if (also) {
        /* Verbose/debug: scan all blocks (slow on large arenas). */
        for (char *c = s_arenaStart; c < s_arenaEnd; ) {
            size_t size = *(size_t *)c;
            if (size < 16 || c + size > s_arenaEnd)
                break;
            if (((arena::Head *)c)->magic == arena::kMagic) {
                char text[33] = {};
                const char *data = c + sizeof(arena::Head);
                for (unsigned j = 0; j < 32; j++)
                    text[j] = (data[j] >= 32 && data[j] < 127) ? data[j] : '.';
                logging::LogLine("[heap] arena live +%zu KB size %zu: %s",
                                 (size_t)(c - s_arenaStart) >> 10, size, text);
                fprintf(also, "[heap] arena live +%zu KB size %zu: %s\n",
                        (size_t)(c - s_arenaStart) >> 10, size, text);
            }
            c += size;
        }
    }
    char *top = arena::TrimEnd(s_arenaEnd);
    /* Log the highest live block (pinning point) for debugging leaks. */
    {
        char *highest = s_arenaStart;
        for (char *c = s_arenaStart; c < s_arenaEnd; ) {
            size_t size = *(size_t *)c;
            if (size < 16 || c + size > s_arenaEnd) break;
            if (((arena::Head *)c)->magic == arena::kMagic) {
                if (c + size > highest) highest = c + size;
            }
            c += size;
        }
        if (highest > s_arenaStart)
            logging::LogLine("[heap] highest live block ends at +%zu KB (top free tail +%zu KB)", (size_t)(highest - s_arenaStart) >> 10, (size_t)(top - s_arenaStart) >> 10);
    }
    /* If the entire arena is free, reset it fast (avoids stale free-list state). */
    if (top == s_arenaStart) {
        arena::g_free = nullptr;
        s_arenaEnd = s_arenaStart;
        top = s_arenaEnd;
    }
    char *targetEnd = fake_heap_start + (((size_t)(top - fake_heap_start) + 0x1FFFFFu) & ~(size_t)0x1FFFFFu);
    if (targetEnd < fake_heap_start + kHeapFloor)
        targetEnd = fake_heap_start + kHeapFloor;
    if (targetEnd < top || targetEnd > s_arenaEnd)
        targetEnd = s_arenaEnd;
    if (targetEnd > top)
        arena::Insert(top, (size_t)(targetEnd - top));

    void *addr = nullptr;
    if (targetEnd < s_arenaEnd && R_SUCCEEDED(svcSetHeapSize(&addr, (size_t)(targetEnd - fake_heap_start)))) {
        if (addr == (void *)fake_heap_start || g_heapSize == 0) {
            s_arenaEnd = targetEnd;
            g_heapSize = (size_t)(targetEnd - fake_heap_start);
            if (s_arenaEnd == s_arenaStart)
                arena::g_free = nullptr;
        } else {
            arena::Insert(targetEnd, (size_t)(s_arenaEnd - targetEnd));
        }
    } else if (targetEnd < s_arenaEnd) {
        arena::Insert(targetEnd, (size_t)(s_arenaEnd - targetEnd));
    } else if (s_arenaEnd == s_arenaStart) {
        arena::g_free = nullptr;
    }
    mutexUnlock(&s_arenaLock);
    return (unsigned)(g_heapSize >> 20);
}

static Result heap_set_floor() {
    /* Reclaim arena, then reclaim the full floor heap; RC failures are common and tolerated. */
    if (s_arenaReady)
        heap_release_arena(nullptr);
    return heap_set_total(kHeapFloor);
}

static Result heap_set_cap() {
    return heap_set_total(kHeapCap);
}

static Result heap_set_burst() {
    return heap_set_total(kHeapBurst);
}

enum class HeapProfile : int {
    Minimal = 0,
    Cap = 1,
    Burst = 2,
};

extern "C" int qext_heap_set(int profile)
{
    static Result s_lastFailRc = 0;
    static int s_lastFailProfile = -1;
    static size_t s_lastFailSize = (size_t)-1;
    Result rc;
    if (profile == (int)HeapProfile::Burst)
        rc = heap_set_burst();
    else if (profile == (int)HeapProfile::Cap)
        rc = heap_set_cap();
    else
        rc = heap_set_floor();
    if (R_FAILED(rc)) {
        if (rc != s_lastFailRc || profile != s_lastFailProfile || g_heapSize != s_lastFailSize) {
            logging::LogLine("[heap] set profile %d failed (rc=0x%X), size=%zu",
                             profile, rc, g_heapSize);
            s_lastFailRc = rc;
            s_lastFailProfile = profile;
            s_lastFailSize = g_heapSize;
        }
    } else {
        s_lastFailRc = 0;
        s_lastFailProfile = -1;
        s_lastFailSize = (size_t)-1;
    }
    return (int)rc;
}

extern "C" int qext_heap_set_size(unsigned size_mb)
{
    size_t target = (size_t)size_mb * 1024u * 1024u;
    return (int)heap_set_total(target);
}

extern "C" unsigned qext_heap_release(void)
{
    return heap_release_arena(nullptr);
}

extern "C" unsigned long qext_arena_free_bytes(void)
{
    if (!s_arenaReady)
        return 0;
    mutexLock(&s_arenaLock);
    size_t n = arena::FreeBytes();
    mutexUnlock(&s_arenaLock);
    return (unsigned long)n;
}

extern "C" unsigned long qext_arena_size(void)
{
    if (!s_arenaReady)
        return 0;
    return (unsigned long)(s_arenaEnd - s_arenaStart);
}

extern "C" unsigned qext_heap_size_mb(void)
{
    return (unsigned)(g_heapSize >> 20);
}

/* Arena-backed allocator overrides (all malloc routed to arena when ready). */
extern "C" {

void *malloc(size_t n)
{
    void *p = ArenaAlloc(n ? n : 1, 16);
    return p ? p : _malloc_r(_REENT, n);
}

void free(void *p)
{
    if (!p)
        return;
    if (InArena(p)) {
        mutexLock(&s_arenaLock);
        arena::Release(p);
        mutexUnlock(&s_arenaLock);
        return;
    }
    _free_r(_REENT, p);
}

void *calloc(size_t a, size_t b)
{
    size_t n;
    if (__builtin_mul_overflow(a, b, &n))
        return nullptr;
    void *p = ArenaAlloc(n ? n : 1, 16);
    if (p) {
        memset(p, 0, n);
        return p;
    }
    return _calloc_r(_REENT, a, b);
}

void *memalign(size_t align, size_t n)
{
    void *p = ArenaAlloc(n ? n : 1, align);
    return p ? p : _memalign_r(_REENT, align, n);
}

void *aligned_alloc(size_t align, size_t n)
{
    if (align == 0 || (n % align) != 0)
        return nullptr;
    return memalign(align, n);
}

int posix_memalign(void **out, size_t align, size_t n)
{
    if (align < sizeof(void *) || (align & (align - 1)) != 0)
        return 22;
    void *p = memalign(align, n);
    if (!p)
        return 12; /* ENOMEM */
    *out = p;
    return 0;
}

size_t malloc_usable_size(void *p)
{
    if (!p)
        return 0;
    return InArena(p) ? arena::Usable(p) : _malloc_usable_size_r(_REENT, p);
}

void *realloc(void *p, size_t n)
{
    if (!p)
        return malloc(n);
    if (n == 0) {
        free(p);
        return nullptr;
    }
    const size_t old = malloc_usable_size(p);
    if (!InArena(p) && !s_arenaReady)
        return _realloc_r(_REENT, p, n);
    if (InArena(p) && n <= old)
        return p;
    void *q = malloc(n);
    if (!q)
        return nullptr;
    memcpy(q, p, old < n ? old : n);
    free(p);
    return q;
}

} /* extern "C" malloc overrides */

#define FB_W 1280
#define FB_H 720

/* Default framebuffer size */
int g_fbW = FB_W;
int g_fbH = FB_H;

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
static void *g_sdl;
static void *g_ffmpeg;
static void *g_menu;
static void *g_miniini;
static void *g_json;
static void *g_xxhash;

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

        /* Scale layer to logical resolution */
        s32 lw = 0, lh = 0;
        if (R_SUCCEEDED(viGetDisplayLogicalResolution(&g_display, &lw, &lh)) &&
            lw >= 640 && lh >= 360 && lw <= 7680 && lh <= 4320) {
            g_fbW = lw;
            g_fbH = lh;
        }

        /* Set the layer size to the new resolution*/
        viSetLayerSize(&g_layer, (s32)g_fbW, (s32)g_fbH);

        /* Use a low priority to avoid overwriting other system UI */
        /* 0 is probably used for the Quick Settings/notifications */
        /* And 1 is likely used for swkbd */
        viSetLayerZ(&g_layer, 2);

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

    const char **names = (const char **)malloc((size_t)nchain * sizeof(*names));
    void **addrs = (void **)malloc((size_t)nchain * sizeof(*addrs));
    if (!names || !addrs) {
        free(names);
        free(addrs);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
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

        names[count] = name;
        addrs[count] = (void *)dyn_fix(base, s->st_value);
        count++;
    }

    Result brc = dlink_provide_bulk(names, addrs, count);
    free(names);
    free(addrs);
    if (R_FAILED(brc))
        return brc;

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
    if (g_sdl) {
        dlclose(g_sdl);
        g_sdl = nullptr;
    }
    if (g_ffmpeg) {
        dlclose(g_ffmpeg);
        g_ffmpeg = nullptr;
    }
    if (g_neko) {
        dlclose(g_neko);
        g_neko = nullptr;
    }
    if (g_miniini) {
        dlclose(g_miniini);
        g_miniini = nullptr;
    }
    if (g_json) {
        dlclose(g_json);
        g_json = nullptr;
    }
    if (g_xxhash) {
        dlclose(g_xxhash);
        g_xxhash = nullptr;
    }
}

constexpr char kMenuIni[] = "sdmc:/qlaunch-ext/config/menu.ini";
constexpr char kDefaultMenu[] = "sdmc:/qlaunch-ext/menus/menu.dnro";

static void resolve_menu_path(char *out, size_t cap)
{
    snprintf(out, cap, "%s", kDefaultMenu);
    out[cap - 1] = 0;
    if (!g_miniini)
        return;
    auto ini_gets = (int (*)(const char *, const char *, const char *, char *, int, const char *))dlsym(g_miniini, "ini_gets");
    if (!ini_gets)
        return;
    ini_gets("menu", "path", "", out, (int)cap, kMenuIni);
    out[cap - 1] = 0;
    if (!out[0])
        snprintf(out, cap, "%s", kDefaultMenu);
    out[cap - 1] = 0;
}

/* One boot stage per part of the initcode, so we can keep applet services silent */
enum BootStage {
    BootStage_Provide,
    BootStage_Layer,
    BootStage_Plugins,
    BootStage_Neko,
    BootStage_Sdl,
    BootStage_Ffmpeg,
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
    if (s_onShutdown)
        s_onShutdown();
    menu_unload();
    g_bootStage = BootStage_Provide;
    static int fails = 0;
    if (++fails == 3)
        logging::LogLine("[qlaunch-ext] Can't open menu");
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
            g_bootStage = BootStage_Plugins;
            break;
        case BootStage_Plugins:
            dlink_set_library_dir(NULL);
            if (!g_miniini) {
                g_miniini = dlopen("miniini", RTLD_GLOBAL);
                logging::LogLine("[qlaunch-ext] opening miniini at %p (dlerr=%s)", g_miniini,
                                 g_miniini ? "NONE" : dlerror());
            }
            if (!g_json) {
                g_json = dlopen("json", RTLD_GLOBAL);
                logging::LogLine("[qlaunch-ext] opening json at %p (dlerr=%s)", g_json,
                                 g_json ? "NONE" : dlerror());
            }
            if (!g_xxhash) {
                g_xxhash = dlopen("xxhash", RTLD_GLOBAL);
                logging::LogLine("[qlaunch-ext] opening xxhash at %p (dlerr=%s)", g_xxhash,
                                 g_xxhash ? "NONE" : dlerror());
            }
            /* Load plugins very early */
            plugins::Boot(&g_win);
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
            g_bootStage = BootStage_Sdl;
            break;
        case BootStage_Sdl:
            dlink_set_library_dir(NULL);
            if (!g_sdl) {
                g_sdl = dlopen("sdl3", RTLD_GLOBAL);
                logging::LogLine("[qlaunch-ext] opening sdl3 at %p (dlerr=%s)", g_sdl,
                                 g_sdl ? "NONE" : dlerror());
                if (!g_sdl)
                    return boot_fail(MAKERESULT(Module_Libnx, LibnxError_NotFound));
            }
            g_bootStage = BootStage_Ffmpeg;
            break;
        case BootStage_Ffmpeg:
            dlink_set_library_dir(NULL);
            if (!g_ffmpeg) {
                g_ffmpeg = dlopen("ffmpeg", RTLD_GLOBAL);
                logging::LogLine("[qlaunch-ext] opening ffmpeg at %p (dlerr=%s)", g_ffmpeg,
                                 g_ffmpeg ? "NONE" : dlerror());
                if (!g_ffmpeg)
                    return boot_fail(MAKERESULT(Module_Libnx, LibnxError_NotFound));
            }
            g_bootStage = BootStage_Menu;
            break;
        case BootStage_Menu: {
            if (g_menu) {
                dlclose(g_menu);
                g_menu = nullptr;
            }
            char menuPath[256];
            resolve_menu_path(menuPath, sizeof(menuPath));
            g_menu = dlopen(menuPath, RTLD_LOCAL);
            logging::LogLine("[qlaunch-ext] opening menu '%s' at %p (dlerr=%s)", menuPath, g_menu,
                             g_menu ? "NONE" : dlerror());
            if (!g_menu)
                return boot_fail(MAKERESULT(Module_Libnx, LibnxError_NotFound));
            g_bootStage = BootStage_Syms;
            break;
        }
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
    nifmInitialize(NifmServiceType_System);
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

    album::Init();
}

extern "C" void __appExit(void)
{
    /* Exit logger */
    logging::LogLine("[qlaunch-ext] exiting");
    logging::Exit();

    /* Exit services */
    album::Exit();
    gfx_exit();
    viExit();
    hidExit();
    lblExit();
    nifmExit();
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
    titles::Refresh(); album::Refresh();
    if (g_booted && s_onOpen)
        s_onOpen();
    plugins::OpenAll();
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
    plugins::PowerAll();
    sys::EnterSleep();
}

static void pump_general_channel(void)
{
    using msg::GeneralChannelMessage;

    for (int i = 0; i < 16; i++) {
        AppletStorage st{};
        if (R_FAILED(appletPopFromGeneralChannel(&st)))
            return;
        msg::SystemAppletMessageHeader h{};
        s64 sz = 0;
        appletStorageGetSize(&st, &sz);
        if (sz >= (s64)sizeof(h))
            appletStorageRead(&st, 0, &h, sizeof(h));
        appletStorageClose(&st);
        if (!h.IsValid()) {
            logging::LogLine("[qlaunch-ext] sams invalid magic=0x%X sz=%lld", h.magic,
                             (long long)sz);
            continue;
        }
        logging::LogLine("[qlaunch-ext] sams msg=%u t=%llu", (u32)h.msg,
                         (unsigned long long)now_ms());
        switch (h.msg) {
        case GeneralChannelMessage::RequestHomeMenu:
            do_home("sams");
            break;
        case GeneralChannelMessage::Sleep:
            do_sleep("sams");
            break;
        case GeneralChannelMessage::Shutdown:
            appletStartShutdownSequence();
            break;
        case GeneralChannelMessage::Reboot:
            appletStartRebootSequence();
            break;
        case GeneralChannelMessage::OverlayShown:
            g_overlayShown = true;
            break;
        case GeneralChannelMessage::OverlayHidden:
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
    using msg::AppletMessage;

    constexpr int AppletMessagePumpMax = 32;

    for (int i = 0; i < AppletMessagePumpMax; i++) {
        u32 raw = 0;
        if (R_FAILED(appletGetMessage(&raw)))
            return;
        logging::LogLine("[qlaunch-ext] ae msg=%u t=%llu", raw, (unsigned long long)now_ms());

        switch ((AppletMessage)raw) {
        case AppletMessage::ChangeIntoForeground:
            if (!g_displayApproved) {
                appletApproveToDisplay();
                g_displayApproved = true;
            }
            if (g_homePending)
                reopen_menu("fg");
            break;
        case AppletMessage::ChangeIntoBackground:
            break;
        case AppletMessage::ApplicationExited:
            titles::Refresh();
            album::Refresh();
            if (g_booted)
                g_menuRefreshPending = true;
            break;
        case AppletMessage::FocusStateChanged:
            break;
        case AppletMessage::DetectShortPressingHomeButton:
            do_home("ae");
            break;
        case AppletMessage::DetectShortPressingPowerButton:
        case AppletMessage::AutoPowerDown:
        case AppletMessage::DetectReceivingCecSystemStandby:
            do_sleep("ae");
            break;
        case AppletMessage::FinishedSleepSequence:
            appletRequestToGetForeground();
            if (g_booted)
                g_menuRefreshPending = true;
            break;
        case AppletMessage::RequestToDisplay:
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
    logging::LogLine("[qlaunch-ext] initialized (built at %s %s)", __DATE__, __TIME__);
    u32 hosVersion = hosversionGet();
    logging::LogLine("[qlaunch-ext] fw %u.%u.%u", HOSVER_MAJOR(hosVersion), HOSVER_MINOR(hosVersion), HOSVER_MICRO(hosVersion));
    Result iprc = appletLoadAndApplyIdlePolicySettings();
    logging::LogLine("[qlaunch-ext] Applied idle policy settings (rc=0x%X)", iprc);

    /* Drain anything queued during boot. */
    pump_general_channel();
    pump_applet_messages();
    static bool was_active = false;
    static bool was_overlay = false;

    while (true) {
        pump_general_channel();
        pump_applet_messages();
        /* game exit */
        bool active = app::IsActive();
        if (was_active && !active) {
            app::Close();
            logging::LogLine("[qlaunch-ext] game exited t=%llu",
                             (unsigned long long)now_ms());
            titles::Refresh(); album::Refresh();
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
            /* Persistent plugins keep ticking even when a game is loaded. */
            plugins::Loop(true);
            svcSleepThread(16000000ULL);
            continue;
        }
        if (g_menuRefreshPending && g_booted) {
            g_menuRefreshPending = false;
            if (s_onOpen)
                s_onOpen();
            plugins::OpenAll();
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
        plugins::Loop(false);
    }

    if (g_booted) {
        if (s_onShutdown)
            s_onShutdown();
        menu_unload();
    }
    plugins::Shutdown();
    gfx_exit();
    /* Stop our heap */
    heap_set_floor();
    return 0;
}
