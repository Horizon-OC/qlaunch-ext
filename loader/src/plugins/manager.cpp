/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering...         */

#include "manager.hpp"
#include "vtitles.hpp"
#include <dlink/dlink.h>
#include <shared/logging.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace plugins {

namespace {

constexpr char kDir[] = "sdmc:/qlaunch-ext/plugins";
constexpr char kBase[] = "sdmc:/qlaunch-ext";
constexpr char kJson[] = "sdmc:/qlaunch-ext/plugins.json";

constexpr int kMaxPlugins = 32;
constexpr int kNameCap = 63;
constexpr int kMaxJsonEntries = 32;

struct Plugin {
    char name[kNameCap + 1];
    void *handle;
    int (*loop)(void);
    void (*shutdown)(void);
    void (*open)(void);
    void (*power)(void);
    bool persistent;
};

struct JsonEntry {
    char name[kNameCap + 1];
    bool enabled;
};

static Plugin s_plugins[kMaxPlugins];
static int s_count = 0;
static bool s_booted = false;

static bool IsDnro(const char *s)
{
    char c = *s;
    return (c == 'd' || c == 'D') && (s[1] == 'n' || s[1] == 'N') &&
           (s[2] == 'r' || s[2] == 'R') && (s[3] == 'o' || s[3] == 'O') &&
           s[4] == 0;
}

static bool HasSuffixDnro(const char *name)
{
    size_t n = strlen(name);
    if (n < 6 || n > (size_t)kNameCap)
        return false;
    if (name[n - 5] != '.')
        return false;
    return IsDnro(name + n - 4);
}

static bool NameOk(const char *name)
{
    if (!name || !name[0])
        return false;
    for (const char *p = name; *p; p++) {
        char c = *p;
        if (c == '/' || c == '\\' || c == '"' || c == ':')
            return false;
        if ((unsigned char)c < 32)
            return false;
    }
    if (strstr(name, "..") != nullptr)
        return false;
    return true;
}

    /// @brief Copy a plugin file name with truncation safety.
    /// @param dst Destination buffer ([kNameCap + 1]).
    /// @param src Source name.
    /// @return True when the whole name fit.
    static bool CopyName(char *dst, const char *src)
    {
        if (!dst || !src)
            return false;
        size_t len = strlen(src);
        if (len == 0 || len > (size_t)kNameCap)
            return false;
        memcpy(dst, src, len + 1);
        return true;
    }

    /// @brief Join the plugin dir and a file name into a path.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @param name Plugin file name (length-checked).
    /// @return True when the whole path fit.
    static bool JoinPath(char *out, unsigned cap, const char *name)
    {
        if (!out || cap == 0 || !name)
            return false;
        size_t dl = strlen(kDir);
        size_t nl = strlen(name);
        if (nl == 0 || nl > (size_t)kNameCap)
            return false;
        if (dl + 1 + nl + 1 > cap)
            return false;
        memcpy(out, kDir, dl);
        out[dl] = '/';
        memcpy(out + dl + 1, name, nl + 1);
        return true;
    }

    static int ScanDir(char names[][kNameCap + 1], int cap)
    {
        int n = 0;
        DIR *d = opendir(kDir);
        if (!d)
            return 0;
        for (struct dirent *e = readdir(d); e; e = readdir(d)) {
            if (n >= cap)
                break;
            if (e->d_name[0] == '.')
                continue;
            if (!HasSuffixDnro(e->d_name))
                continue;
            if (!NameOk(e->d_name))
                continue;
            if (!CopyName(names[n], e->d_name))
                continue;
            n++;
        }
        closedir(d);
        return n;
    }

    /// @brief Build a JSON pointer for an object key, escaping '~'.
    /// @param key Object key.
    /// @param out Output buffer (needs room for '/' plus doubling).
    /// @param cap Buffer capacity.
    /// @return True when the pointer fit.
    static bool KeyPtr(const char *key, char *out, unsigned cap)
    {
        if (!key || !out || cap < 4)
            return false;
        out[0] = '/';
        unsigned pos = 1;
        for (size_t i = 0; key[i] != 0; i++) {
            if (pos + 2 >= cap)
                return false;
            if (key[i] == '~') {
                out[pos++] = '~';
                out[pos++] = '0';
            } else {
                out[pos++] = key[i];
            }
        }
        out[pos] = 0;
        return true;
    }

static int ParseJson(const char *text, size_t len, JsonEntry *out, int cap)
{
    auto object_size = (int (*)(const char *, size_t, const char *))dlsym(NULL, "json_object_size");
    auto key_at = (int (*)(const char *, size_t, const char *, int, char *, size_t))dlsym(NULL, "json_key_at");
    auto get_type = (int (*)(const char *, size_t, const char *))dlsym(NULL, "json_type");
    auto get_bool = (int (*)(const char *, size_t, const char *, int))dlsym(NULL, "json_get_bool");
    if (!object_size || !key_at || !get_type || !get_bool)
        return 0;
    int total = object_size(text, len, "");
    if (total < 0)
        return 0;
    int n = 0;
    char key[kNameCap + 1];
    char ptr[kNameCap * 2 + 4];
    for (int i = 0; i < total && n < cap; i++) {
        if (key_at(text, len, "", i, key, sizeof(key)) < 0)
            break;
        if (!NameOk(key) || !HasSuffixDnro(key))
            continue;
        if (!KeyPtr(key, ptr, sizeof(ptr)))
            continue;
        if (get_type(text, len, ptr) != 1)
            break;
        bool val = get_bool(text, len, ptr, 0) != 0;
        bool dup = false;
        for (int k = 0; k < n; k++) {
            if (strcmp(out[k].name, key) == 0) {
                dup = true;
                break;
            }
        }
        if (dup)
            continue;
        if (!CopyName(out[n].name, key))
            continue;
        out[n].enabled = val;
        n++;
    }
    return n;
}

static int ReadJson(JsonEntry *out, int cap)
{
    FILE *f = fopen(kJson, "rb");
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    int n = 0;
    if (sz > 0 && sz <= 65536) {
        char *buf = (char *)malloc((unsigned)sz);
        if (buf) {
            size_t got = fread(buf, 1, (unsigned)sz, f);
            if (got == (unsigned)sz)
                n = ParseJson(buf, (unsigned)sz, out, cap);
            free(buf);
        }
    }
    fclose(f);
    return n;
}

static void WriteJson(const JsonEntry *ents, int n)
{
    FILE *f = fopen(kJson, "wb");
    if (!f) {
        logging::LogLine("[plugins] cannot write %s", kJson);
        return;
    }
    fputs("{\n", f);
    for (int i = 0; i < n; i++) {
        fprintf(f, "\"%s\": %s%s\n", ents[i].name,
                ents[i].enabled ? "true" : "false",
                (i + 1 < n) ? "," : "");
    }
    fputs("}\n", f);
    fclose(f);
}

static int FindEntry(const JsonEntry *ents, int n, const char *name)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(ents[i].name, name) == 0)
            return i;
    }
    return -1;
}

static bool OnDisk(const char names[][kNameCap + 1], int n, const char *name)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static void UnloadAt(int idx)
{
    if (idx < 0 || idx >= s_count)
        return;
    Plugin *p = &s_plugins[idx];
    if (p->shutdown)
        p->shutdown();
    vtitles::Unregister(p->handle);
    if (p->handle)
        dlclose(p->handle);
    logging::LogLine("[plugins] unloaded %s", p->name);
    s_count--;
    if (idx != s_count)
        s_plugins[idx] = s_plugins[s_count];
    memset(&s_plugins[s_count], 0, sizeof(s_plugins[s_count]));
}

    /// @brief Load one plugin file and run its boot hook.
    /// @param name Plugin file name (length-checked).
    /// @param win Native window handle for the boot hook.
    /// @return True on success.
    static bool LoadOne(const char *name, NWindow *win)
    {
        if (!NameOk(name)) {
            logging::LogLine("[plugins] Bad plugin name, skipping");
            return false;
        }
        if (s_count >= kMaxPlugins) {
            logging::LogLine("[plugins] Can't load more plugins, skipping %s", name);
            return false;
        }
        char path[128];
        if (!JoinPath(path, sizeof(path), name)) {
            logging::LogLine("[plugins] Path too long, skipping %s", name);
            return false;
        }
        void *h = dlopen(path, RTLD_LOCAL);
        logging::LogLine("[plugins] Opening %s at %p (dlerr=%s)", name, h,
                         h ? "NONE" : dlerror());
        if (!h)
            return false;
        Plugin p{};
        CopyName(p.name, name);
        p.handle = h;
        p.loop = (int (*)(void))dlsym(h, "loop");
        if (!p.loop) {
            logging::LogLine("[plugins] %s has no loop, skipping", name);
            dlclose(h);
            return false;
        }
        p.shutdown = (void (*)(void))dlsym(h, "onShutdown");
        p.open = (void (*)(void))dlsym(h, "onOpen");
        p.power = (void (*)(void))dlsym(h, "onPowerButton");
        bool (*isPers)(void) = (bool (*)(void))dlsym(h, "IsPersistent");
        p.persistent = isPers ? isPers() : false;
        int (*onBoot)(NWindow *) = (int (*)(NWindow *))dlsym(h, "onBoot");
        if (onBoot) {
            int rc = onBoot(win);
            if (rc != 0) {
                if (p.shutdown)
                    p.shutdown();
                dlclose(h);
                return false;
            }
        } else {
            logging::LogLine("[plugins] %s loaded (Persistent = %d)", name,
                             (int)p.persistent);
        }
    s_plugins[s_count++] = p;
    vtitles::Register(h);
    /* Inventory record (not a diagnostic): identifies the plugin's
       TitlePlugin interface generation on-device. */
    {
        char iface[16];
        int pos = 0;
        const char tags[] = "CTDONIiLR";
        const char *names[] = { "TitleCount", "TitleTid", "TitleDonor", "TitleNro", "TitleName",
                                "TitleIconSize", "TitleIcon", "TitleLaunch", "TitleRefresh" };
        for (int k = 0; k < 9; k++) {
            if (dlsym(h, names[k]) && pos + 1 < (int)sizeof(iface))
                iface[pos++] = tags[k];
        }
        iface[pos] = 0;
        logging::LogLine("[plugins] %s titles=[%s]", name, iface);
    }
    return true;
}

} /* namespace */

void Boot(NWindow *win)
{
    if (s_booted)
        return;
    s_booted = true;

    mkdir(kBase, 0777);
    mkdir(kDir, 0777);

    char disk[kMaxJsonEntries][kNameCap + 1];
    int nDisk = ScanDir(disk, kMaxJsonEntries);
    logging::LogLine("[plugins] Found %d plugins in %s", nDisk, kDir);

    JsonEntry ents[kMaxJsonEntries];
    int nEnts = ReadJson(ents, kMaxJsonEntries);
    bool dirty = false;
    if (nEnts < 0) {
        nEnts = 0;
        dirty = true; /* no config yet */
    }
    for (int i = 0; i < nDisk && nEnts < kMaxJsonEntries; i++) {
        if (FindEntry(ents, nEnts, disk[i]) < 0) {
            if (!CopyName(ents[nEnts].name, disk[i]))
                continue;
            ents[nEnts].enabled = true; /* new plugins default to on */
            nEnts++;
            dirty = true;
            logging::LogLine("[plugins] New plugin %s has been enabled",
                             disk[i]);
        }
    }
    if (dirty)
        WriteJson(ents, nEnts);

    for (int i = 0; i < nEnts; i++) {
        if (!ents[i].enabled)
            continue;
        if (!OnDisk(disk, nDisk, ents[i].name)) {
            logging::LogLine("[plugins] %s enabled but not on SD",
                             ents[i].name);
            continue;
        }
        LoadOne(ents[i].name, win);
    }
    logging::LogLine("[plugins] Started plugins. %d currently active", s_count);
}

void Loop(bool inGame)
{
    for (int i = 0; i < s_count;) {
        Plugin *p = &s_plugins[i];
        if (inGame && !p->persistent) {
            i++;
            continue;
        }
        int rc = p->loop();
        if (rc != 0) {
            logging::LogLine("[plugins] %s: Loop failed with RC=%d, unloading", p->name,
                             rc);
            UnloadAt(i);
            continue;
        }
        i++;
    }
}

void OpenAll()
{
    for (int i = 0; i < s_count; i++) {
        if (s_plugins[i].open)
            s_plugins[i].open();
    }
}

void PowerAll()
{
    for (int i = 0; i < s_count; i++) {
        if (s_plugins[i].power)
            s_plugins[i].power();
    }
}

void Shutdown()
{
    for (int i = s_count - 1; i >= 0; i--)
        UnloadAt(i);
    s_count = 0;
}

} /* namespace plugins */

