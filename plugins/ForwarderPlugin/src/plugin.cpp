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

/* Provided by the loader */
namespace logging {
    void LogLine(const char *fmt, ...);
}

extern "C" uint64_t xxh64(const void *data, size_t len, uint64_t seed);

namespace fwd {

    constexpr char kPluginConfigDir[] = "sdmc:/qlaunch-ext/config/plugin/forwarder.ini";
    constexpr char kForwarderConfigDir[] = "sdmc:/qlaunch-ext/forwarders";
    constexpr char kIconCacheDir[] = "sdmc:/qlaunch-ext/cache/nroicon";
    constexpr u64 kVirtualBit = 0x8000000000000000ULL;
    constexpr unsigned kIconCap = 0x40000;
    constexpr int kMaxFiles = 64;
    constexpr int kMaxForwarders = 32;

    struct Forwarder {
        char name[192] = { 0 };
        char nro[512] = { 0 };
        char icon[256] = { 0 };
        char version[32] = { 0 };
        char publisher[192] = { 0 };
    };

    static u64 s_DonorTitle = 0;
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

    /// @brief Hash a tile name into a deterministic virtual tid (XXH64).
    /// @param name Tile name (non-empty).
    /// @return Virtual tid with the high bit set.
    static u64 HashName(const char *name)
    {
        u64 h = (u64)xxh64(name, strlen(name), 0);
        u64 vtid = h | kVirtualBit;
        if (vtid == kVirtualBit)
            vtid |= 1;
        return vtid;
    }

    /// @brief Read the shared donor from the plugin config.
    static void ReadPluginConfig()
    {
        s_DonorTitle = 0;
        mkdir("sdmc:/qlaunch-ext", 0777);
        mkdir("sdmc:/qlaunch-ext/config", 0777);
        mkdir("sdmc:/qlaunch-ext/config/plugin", 0777);

        char donor[32];
        ini_gets("forwarder", "donor", "", donor, (int)sizeof(donor), kPluginConfigDir);
        s_DonorTitle = ParseTid(donor);
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

    /// @brief Fill a tile entry from the NRO when INI values are blank.
    /// @param out Entry to fill
    static void FillFromNro(Forwarder *out);

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
        ini_gets("forwarder", "version", "", out->version, (int)sizeof(out->version), path);
        ini_gets("forwarder", "publisher", "", out->publisher, (int)sizeof(out->publisher), path);
        
        if (out->publisher[0] == 0)
            ini_gets("forwarder", "author", "", out->publisher, (int)sizeof(out->publisher), path);
        
        if (out->name[0] == 0 || out->nro[0] == 0)
            return false;
        
        FillFromNro(out);
        return true;
    }

    /// @brief Read an NRO asset section (icon or NACP) into a buffer.
    /// @param nroPath NRO file path.
    /// @param wantIcon True for the icon section, false for NACP.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Bytes read (<0 on error).
    static int ReadNroAsset(const char *nroPath, bool wantIcon, void *out, unsigned cap)
    {
        if (nroPath == nullptr || nroPath[0] == 0 || out == nullptr || cap == 0) {
            return -1;
        }

        FILE *nroFile = fopen(nroPath, "rb");

        if (nroFile == nullptr) {
            return -1;
        }

        NroHeader nroHeader;
        NroAssetHeader assetHeader;

        if (fseek(nroFile, (long)sizeof(NroStart), SEEK_SET) != 0) {
            fclose(nroFile);
            return -1;
        }

        if (fread(&nroHeader, sizeof(nroHeader), 1, nroFile) != 1) {
            fclose(nroFile);
            return -1;
        }

        if (nroHeader.magic != NROHEADER_MAGIC) {
            fclose(nroFile);
            return -1;
        }

        if (fseek(nroFile, (long)nroHeader.size, SEEK_SET) != 0) {
            fclose(nroFile);
            return -1;
        }

        if (fread(&assetHeader, sizeof(assetHeader), 1, nroFile) != 1) {
            fclose(nroFile);
            return -1;
        }

        if (assetHeader.magic != NROASSETHEADER_MAGIC || assetHeader.version > NROASSETHEADER_VERSION) {
            fclose(nroFile);
            return -1;
        }

        const NroAssetSection &section = wantIcon ? assetHeader.icon : assetHeader.nacp;

        if (section.offset == 0 || section.size == 0 || section.size > cap) {
            fclose(nroFile);
            return -1;
        }

        if (fseek(nroFile, (long)(nroHeader.size + section.offset), SEEK_SET) != 0) {
            fclose(nroFile);
            return -1;
        }

        size_t sectionSize = (size_t)section.size;

        if (fread(out, sectionSize, 1, nroFile) != 1) {
            fclose(nroFile);
            return -1;
        }

        fclose(nroFile);
        return (int)section.size;
    }

    /// @brief Copy a possibly unterminated NACP string into a NUL-capped field.
    /// @param dst Destination buffer.
    /// @param dstCap Destination capacity.
    /// @param src NACP bytes (may lack NUL).
    /// @param srcCap Source byte limit.
    /// @return True when anything was copied.
    static bool CopyNacpString(char *dst, unsigned dstCap, const char *src, unsigned srcCap)
    {
        if (dst == nullptr || dstCap == 0 || src == nullptr || srcCap == 0) {
            return false;
        }

        unsigned copyLength = 0;

        while (copyLength + 1 < dstCap && copyLength < srcCap && src[copyLength] != 0) {
            copyLength++;
        }

        memcpy(dst, src, copyLength);
        dst[copyLength] = 0;
        return copyLength > 0;
    }

    /// @brief Extract the NRO icon into the SD cache, pointing the entry at it.
    /// @param entry Entry missing an icon (updated in place).
    static void CacheNroIcon(Forwarder *entry)
    {
        unsigned char *iconBytes = (unsigned char *)malloc(kIconCap);

        if (iconBytes == nullptr) {
            return;
        }

        int iconSize = ReadNroAsset(entry->nro, true, iconBytes, kIconCap);

        if (iconSize > 2) {
            mkdir("sdmc:/qlaunch-ext/cache", 0777);
            mkdir(kIconCacheDir, 0777);

            char cachePath[320];
            int pathLength = snprintf(cachePath, sizeof(cachePath), "%s/fwd_%016llX.jpg", kIconCacheDir,
                                      (unsigned long long)HashName(entry->nro));

            if (pathLength > 0 && (unsigned)pathLength < sizeof(cachePath)) {
                FILE *cacheFile = fopen(cachePath, "wb");

                if (cacheFile != nullptr) {
                    size_t writtenBytes = fwrite(iconBytes, 1, (unsigned)iconSize, cacheFile);
                    fclose(cacheFile);

                    if (writtenBytes == (unsigned)iconSize) {
                        snprintf(entry->icon, sizeof(entry->icon), "%s", cachePath);
                    }
                }
            }
        }

        free(iconBytes);
    }

    /// @brief Fill blank version/publisher from the NRO NACP, icon from the NRO.
    /// INI values always win; entries are updated in place.
    /// @param entry Entry to fill.
    static void FillFromNro(Forwarder *entry)
    {
        if (entry->icon[0] == 0)
            CacheNroIcon(entry);

        if (entry->version[0] != 0 && entry->publisher[0] != 0)
            return;

        NacpStruct *nacp = (NacpStruct *)malloc(sizeof(NacpStruct));
        if (!nacp)
            return;

        int nacpSize = ReadNroAsset(entry->nro, false, nacp, sizeof(NacpStruct));
        if (nacpSize >= (int)sizeof(NacpStruct)) {
            if (entry->version[0] == 0) {
                CopyNacpString(entry->version, sizeof(entry->version),
                               nacp->display_version, sizeof(nacp->display_version));
            }

            if (entry->publisher[0] == 0 && nacp->titles_data_format == 0) {
                NacpLanguageEntry *langEntry = nullptr;
                if (R_FAILED(nacpGetLanguageEntry(nacp, &langEntry)) || !langEntry)
                    langEntry = &nacp->lang_data.lang[0];

                if (langEntry) {
                    CopyNacpString(entry->publisher, sizeof(entry->publisher),
                                   langEntry->author, sizeof(langEntry->author));
                }
            }
        }

        free(nacp);
    }

    /// @brief Re-read the donor config and all tile INIs.
    static void ReadConfig()
    {
        ReadPluginConfig();
        s_count = 0;
        if (s_DonorTitle == 0) {
            if (!s_loggedNoDonor) {
                s_loggedNoDonor = true;
                logging::LogLine("[fwd] ERROR: No donor set, can't initialize");
            }
            s_loggedDonor = 0;
            s_loggedCount = -1;
            return;
        }
        s_loggedNoDonor = false;

        mkdir(kForwarderConfigDir, 0777);
        static char files[kMaxFiles][256];

        int cfgFileCount = 0;
        DIR *dir = opendir(kForwarderConfigDir);
        if (dir) {
            for (struct dirent *e = readdir(dir); e; e = readdir(dir)) {
                if (e->d_name[0] == '.' || !IsIni(e->d_name))
                    continue;
                
                if (cfgFileCount >= kMaxFiles)
                    break;
                
                snprintf(files[cfgFileCount], sizeof(files[cfgFileCount]), "%s", e->d_name);
                cfgFileCount++;
            }
            closedir(dir);
        }

        for (int i = 1; i < cfgFileCount; i++) {
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
        for (int i = 0; i < cfgFileCount && s_count < kMaxForwarders; i++) {
            int w = snprintf(path, sizeof(path), "%s/%s", kForwarderConfigDir, files[i]);
            if (w <= 0 || (unsigned)w >= sizeof(path))
                continue;
            if (ParseForwarder(path, &s_forwarders[s_count]))
                s_count++;
        }

        if (s_DonorTitle != s_loggedDonor || s_count != s_loggedCount) {
            s_loggedDonor = s_DonorTitle;
            s_loggedCount = s_count;
            for (int i = 0; i < s_count; i++) {
                logging::LogLine("[fwd] Forwarder '%s' mapped to NRO '%s'", s_forwarders[i].name,
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
        
        size_t bytesRead = fread(out, 1, (unsigned)sz, fp);
        fclose(fp);
        if (bytesRead != (unsigned)sz)
            return -1;
        
        return (int)bytesRead;
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

/* Plugin interface */
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
        return fwd::s_DonorTitle;
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

    int TitleVersion(int index, char *out, unsigned cap)
    {
        const fwd::Forwarder *e = fwd::At(index);
        if (!e || !out || !cap)
            return -1;
        size_t n = strlen(e->version);
        if (n == 0 || n >= cap)
            return -1;
        memcpy(out, e->version, n + 1);
        return (int)n;
    }

    int TitlePublisher(int index, char *out, unsigned cap)
    {
        const fwd::Forwarder *e = fwd::At(index);
        if (!e || !out || !cap)
            return -1;
        size_t n = strlen(e->publisher);
        if (n == 0 || n >= cap)
            return -1;
        memcpy(out, e->publisher, n + 1);
        return (int)n;
    }

} /* extern C */
