/* ForwarderPlugin (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Puts hbmenu on the home menu as a single virtual title ("Homebrew
 * Menu", fake tid 0xE767676767676767) backed by a donor title
 * (default: Tears of the Kingdom). Tapping it boots an NRO
 * (default: sdmc:/hbmenu.nro) under the donor through the loader's
 * versioned NRO payload, so donors stay pristine and no foreign
 * hbl.nsp is needed. The virtual tid only exists menu-side, so no
 * fake title is ever installed or logged. Other forwarders can be
 * added the same way: one INI per tile (name, donor, nro, icon).
 *
 * Config: sdmc:/qlaunch-ext/forwarders/hbmenu.ini
 *   name  = menu label (default "Homebrew Menu")
 *   donor = donor title id, hex (default 0100F2C0115B6000)
 *   nro   = NRO to boot (default sdmc:/hbmenu.nro)
 *   icon  = JPEG tile, forced from sdmc:/qlaunch-ext/icons/ by default
 *           (the loader falls back to the same folder, then to its own
 *           fallback tile) */

#include <switch.h>
#include <qlaunch/plugin.h>
#include <qlaunch/titles.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>

namespace logging {
void LogLine(const char *fmt, ...);
}

namespace fwd {

    constexpr char kCfgPath[] = "sdmc:/qlaunch-ext/forwarders/hbmenu.ini";
    constexpr char kDefaultIcon[] = "sdmc:/qlaunch-ext/icons/E767676767676767.jpg";
    constexpr char kDefaultNro[] = "sdmc:/hbmenu.nro";
    constexpr u64 kFakeTid = 0xE767676767676767ULL;
    constexpr u64 kDefaultDonor = 0x0100F2C0115B6000ULL; /* Tears of the Kingdom */
    constexpr unsigned kIconCap = 0x40000;

    static char s_name[192] = "Homebrew Menu";
    static u64 s_donor = kDefaultDonor;
    static char s_nro[512] = "sdmc:/hbmenu.nro";
    static char s_icon[256] = "sdmc:/qlaunch-ext/icons/E767676767676767.jpg";

    /// @brief Last logged config, so steady state stays quiet.
    static char s_loggedName[192] = { 0 };
    static u64 s_loggedDonor = 0;

    /// @brief Trim leading/trailing whitespace in place.
    /// @param s Line buffer (updated).
    /// @return Trimmed pointer.
    static char *Trim(char *s)
    {
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
            s++;
        size_t n = strlen(s);
        while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                         s[n - 1] == '\r' || s[n - 1] == '\n'))
            s[--n] = 0;
        return s;
    }

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

    /// @brief Read the hbmenu.ini config into the cached settings.
    /// Always logs the resolved config so boot presence is visible.
    static void ReadConfig()
    {
        memcpy(s_name, "Homebrew Menu", sizeof("Homebrew Menu"));
        s_donor = kDefaultDonor;
        memcpy(s_nro, kDefaultNro, sizeof(kDefaultNro));
        memcpy(s_icon, kDefaultIcon, sizeof(kDefaultIcon));
        mkdir("sdmc:/qlaunch-ext", 0777);
        mkdir("sdmc:/qlaunch-ext/forwarders", 0777);
        FILE *f = fopen(kCfgPath, "rb");
        if (f) {
            char buf[1024];
            size_t got = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            buf[got] = 0;
            for (char *line = strtok(buf, "\n"); line; line = strtok(nullptr, "\n")) {
                char *t = Trim(line);
                if (!t[0] || t[0] == '#' || t[0] == ';' || t[0] == '[')
                    continue;
                char *eq = strchr(t, '=');
                if (!eq)
                    continue;
                *eq = 0;
                char *key = Trim(t);
                char *val = Trim(eq + 1);
                if (strcmp(key, "name") == 0 && val[0]) {
                    size_t n = strlen(val);
                    if (n > sizeof(s_name) - 1)
                        n = sizeof(s_name) - 1;
                    memcpy(s_name, val, n);
                    s_name[n] = 0;
                } else if (strcmp(key, "donor") == 0) {
                    u64 tid = ParseTid(val);
                    if (tid != 0)
                        s_donor = tid;
                } else if (strcmp(key, "nro") == 0 && val[0]) {
                    size_t n = strlen(val);
                    if (n > sizeof(s_nro) - 1)
                        n = sizeof(s_nro) - 1;
                    memcpy(s_nro, val, n);
                    s_nro[n] = 0;
                } else if (strcmp(key, "icon") == 0 && val[0]) {
                    size_t n = strlen(val);
                    if (n > sizeof(s_icon) - 1)
                        n = sizeof(s_icon) - 1;
                    memcpy(s_icon, val, n);
                    s_icon[n] = 0;
                }
            }
        }
        /* Announce once per distinct config; steady state stays quiet. */
        if (strcmp(s_name, s_loggedName) != 0 || s_donor != s_loggedDonor) {
            memcpy(s_loggedName, s_name, sizeof(s_name));
            s_loggedDonor = s_donor;
            logging::LogLine("[fwd] '%s' -> donor %016llX nro '%s' (plugin %s %s)", s_name,
                             (unsigned long long)s_donor, s_nro, __DATE__, __TIME__);
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
        FILE *f = fopen(path, "rb");
        if (!f)
            return -1;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz <= 2 || (unsigned)sz > cap || (unsigned)sz > kIconCap) {
            fclose(f);
            return -1;
        }
        size_t got = fread(out, 1, (unsigned)sz, f);
        fclose(f);
        if (got != (unsigned)sz)
            return -1;
        return (int)got;
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
    return 1;
}

unsigned long long TitleTid(int index)
{
    if (index != 0)
        return 0;
    return fwd::kFakeTid;
}

unsigned long long TitleDonor(int index)
{
    if (index != 0)
        return 0;
    return fwd::s_donor;
}

int TitleNro(int index, char *out, unsigned cap)
{
    if (index != 0 || !out || !cap)
        return -1;
    size_t n = strlen(fwd::s_nro);
    if (n == 0 || n >= cap)
        return -1;
    memcpy(out, fwd::s_nro, n + 1);
    return (int)n;
}

int TitleName(int index, char *out, unsigned cap)
{
    if (index != 0 || !out || !cap)
        return -1;
    size_t n = strlen(fwd::s_name);
    if (n == 0 || n >= cap)
        return -1;
    memcpy(out, fwd::s_name, n + 1);
    return (int)n;
}

int TitleIconSize(int index)
{
    if (index != 0 || !fwd::s_icon[0])
        return -1;
    FILE *f = fopen(fwd::s_icon, "rb");
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    if (sz <= 2 || (unsigned)sz > fwd::kIconCap)
        return -1;
    return (int)sz;
}

int TitleIcon(int index, void *out, unsigned cap)
{
    if (index != 0)
        return -1;
    return fwd::ReadIcon(fwd::s_icon, out, cap);
}

void TitleRefresh(void)
{
    fwd::ReadConfig();
}

} /* extern C */
