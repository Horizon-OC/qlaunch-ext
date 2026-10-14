/* ForwarderPlugin (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */

#include <switch.h>
#include <qlaunch/plugin.h>
#include <qlaunch/titles.h>
#include <minIni.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>

namespace logging {
void LogLine(const char *fmt, ...);
}

namespace fwd {

    constexpr char kPluginCfg[] = "sdmc:/qlaunch-ext/config/plugin/forwarder.ini";
    constexpr char kForwarderDir[] = "sdmc:/qlaunch-ext/forwarders";
    constexpr u64 kVirtualBit = 0x8000000000000000ULL;
    constexpr unsigned kIconCap = 0x40000;
    constexpr int kMaxFiles = 64;
    constexpr int kMaxForwarders = 32;

    struct Forwarder {
        char name[192] = { 0 };
        char nro[512] = { 0 };
        char icon[256] = { 0 };
    };

    static u64 s_donor = 0;
    static Forwarder s_forwarders[kMaxForwarders];
    static int s_count = 0;

    /// @brief Last logged donor + tile count, so steady state stays quiet.
    static u64 s_loggedDonor = 0;
    static int s_loggedCount = -1;
    static bool s_loggedNoDonor = false;

    /// @brief Parse a hex title id, tolerating a 0x prefix and case.
    /// @param s Text to parse.
    /// @return Parsed id, or 0 when malformed.
    static u64 ParseTid(const char *s)
    {
        if (!s || !s[0])
            return 0;
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
            s += 2;
        if (!s[0])
            return 0;
        for (const char *p = s; *p; p++) {
            char c = *p;
            bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                       (c >= 'A' && c <= 'F');
            if (!hex)
                return 0;
        }
        return strtoull(s, nullptr, 16);
    }

    /// @brief Hash a tile name into a deterministic virtual tid (FNV-1a).
    /// @param name Tile name (non-empty).
    /// @return Virtual tid with the high bit set.
    static u64 HashName(const char *name)
    {
        u64 h = 14695981039346656037ULL;
        for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
            h ^= (u64)*p;
            h *= 1099511628211ULL;
        }
        u64 vtid = h | kVirtualBit;
        if (vtid == kVirtualBit)
            vtid |= 1;
        return vtid;
    }

    /// @brief Read the shared donor from the plugin config.
    static void ReadPluginConfig()
    {
        s_donor = 0;
        mkdir("sdmc:/qlaunch-ext", 0777);
        mkdir("sdmc:/qlaunch-ext/config", 0777);
        mkdir("sdmc:/qlaunch-ext/config/plugin", 0777);
        char donor[32];
        ini_gets("forwarder", "donor", "", donor, (int)sizeof(donor), kPluginCfg);
        s_donor = ParseTid(donor);
    }

    /// @brief Test whether a file name ends in .ini (any case).
    /// @param name File name.
    /// @return True for INI files.
    static bool IsIni(const char *name)
    {
        size_t n = strlen(name);
        if (n < 5)
            return false;
        const char *ext = name + n - 4;
        return (ext[0] == '.') &&
               (ext[1] == 'i' || ext[1] == 'I') &&
               (ext[2] == 'n' || ext[2] == 'N') &&
               (ext[3] == 'i' || ext[3] == 'I');
    }

    /// @brief Parse one tile INI into an entry.
    /// @param path INI path.
    /// @param out Entry to populate.
    /// @return True when the entry has a name and an NRO.
    static bool ParseForwarder(const char *path, Forwarder *out)
    {
        memset(out, 0, sizeof(*out));
        ini_gets("forwarder", "name", "", out->name, (int)sizeof(out->name), path);
        ini_gets("forwarder", "nro", "", out->nro, (int)sizeof(out->nro), path);
        ini_gets("forwarder", "icon", "", out->icon, (int)sizeof(out->icon), path);
        return out->name[0] != 0 && out->nro[0] != 0;
    }

    /// @brief Re-read the donor config and all tile INIs.
    static void ReadConfig()
    {
        ReadPluginConfig();
        s_count = 0;
        if (s_donor == 0) {
            if (!s_loggedNoDonor) {
                s_loggedNoDonor = true;
                logging::LogLine("[fwd] No donor set (%s)", kPluginCfg);
            }
            s_loggedDonor = 0;
            s_loggedCount = -1;
            return;
        }
        s_loggedNoDonor = false;

        mkdir(kForwarderDir, 0777);
        static char files[kMaxFiles][256];
        int nfiles = 0;
        DIR *d = opendir(kForwarderDir);
        if (d) {
            for (struct dirent *e = readdir(d); e; e = readdir(d)) {
                if (e->d_name[0] == '.' || !IsIni(e->d_name))
                    continue;
                if (nfiles >= kMaxFiles)
                    break;
                snprintf(files[nfiles], sizeof(files[nfiles]), "%s", e->d_name);
                nfiles++;
            }
            closedir(d);
        }
        for (int i = 1; i < nfiles; i++) {
            char tmp[256];
            snprintf(tmp, sizeof(tmp), "%s", files[i]);
            int j = i - 1;
            while (j >= 0 && strcmp(files[j], tmp) > 0) {
                snprintf(files[j + 1], sizeof(files[j + 1]), "%s", files[j]);
                j--;
            }
            snprintf(files[j + 1], sizeof(files[j + 1]), tmp);
        }

        char path[320];
        for (int i = 0; i < nfiles && s_count < kMaxForwarders; i++) {
            int w = snprintf(path, sizeof(path), "%s/%s", kForwarderDir, files[i]);
            if (w <= 0 || (unsigned)w >= sizeof(path))
                continue;
            if (ParseForwarder(path, &s_forwarders[s_count]))
                s_count++;
        }

        if (s_donor != s_loggedDonor || s_count != s_loggedCount) {
            s_loggedDonor = s_donor;
            s_loggedCount = s_count;
            for (int i = 0; i < s_count; i++) {
                logging::LogLine("[fwd] TID '%s' mapped to NRO '%s'", s_forwarders[i].name,
                                 s_forwarders[i].nro);
            }
        }
    }

    /// @brief Read a whole icon file with a size cap.
    /// @param path JPEG file path.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Bytes read (<0 on error).
    static int ReadIcon(const char *path, void *out, unsigned cap)
    {
        if (!path || !path[0] || !out || !cap)
            return -1;
        FILE *fp = fopen(path, "rb");
        if (!fp)
            return -1;
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (sz <= 2 || (unsigned)sz > cap || (unsigned)sz > kIconCap) {
            fclose(fp);
            return -1;
        }
        size_t got = fread(out, 1, (unsigned)sz, fp);
        fclose(fp);
        if (got != (unsigned)sz)
            return -1;
        return (int)got;
    }

    /// @brief Fetch the entry for a title index.
    /// @param index Flat title index.
    /// @return Entry, or null when out of range.
    static const Forwarder *At(int index)
    {
        if (index < 0 || index >= s_count)
            return nullptr;
        return &s_forwarders[index];
    }

} // namespace fwd

extern "C" {

int onBoot(NWindow *win)
{
    (void)win;
    fwd::ReadConfig();
    return 0;
}

int loop(void)
{
    return 0;
}

void onShutdown(void)
{
}

bool IsPersistent(void)
{
    return false;
}

int TitleCount(void)
{
    return fwd::s_count;
}

unsigned long long TitleTid(int index)
{
    const fwd::Forwarder *e = fwd::At(index);
    if (!e)
        return 0;
    return fwd::HashName(e->name);
}

unsigned long long TitleDonor(int index)
{
    if (!fwd::At(index))
        return 0;
    return fwd::s_donor;
}

int TitleNro(int index, char *out, unsigned cap)
{
    const fwd::Forwarder *e = fwd::At(index);
    if (!e || !out || !cap)
        return -1;
    size_t n = strlen(e->nro);
    if (n == 0 || n >= cap)
        return -1;
    memcpy(out, e->nro, n + 1);
    return (int)n;
}

int TitleName(int index, char *out, unsigned cap)
{
    const fwd::Forwarder *e = fwd::At(index);
    if (!e || !out || !cap)
        return -1;
    size_t n = strlen(e->name);
    if (n == 0 || n >= cap)
        return -1;
    memcpy(out, e->name, n + 1);
    return (int)n;
}

int TitleIconSize(int index)
{
    const fwd::Forwarder *e = fwd::At(index);
    if (!e || !e->icon[0])
        return -1;
    FILE *fp = fopen(e->icon, "rb");
    if (!fp)
        return -1;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fclose(fp);
    if (sz <= 2 || (unsigned)sz > fwd::kIconCap)
        return -1;
    return (int)sz;
}

int TitleIcon(int index, void *out, unsigned cap)
{
    const fwd::Forwarder *e = fwd::At(index);
    if (!e)
        return -1;
    return fwd::ReadIcon(e->icon, out, cap);
}

void TitleRefresh(void)
{
    fwd::ReadConfig();
}

} /* extern C */
