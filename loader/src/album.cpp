/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "album.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <shared/logging.hpp>
#include <dirent.h>
#include <sys/stat.h>

namespace album {

namespace {

constexpr int kMaxKeep = 256;
constexpr int kMaxFetch = 512;

CapsAlbumEntry s_list[kMaxKeep];
int s_count = 0;
bool s_ok = false;

int CmpDate(const CapsAlbumFileDateTime &a, const CapsAlbumFileDateTime &b)
{
    if (a.year != b.year) return (a.year > b.year) ? 1 : -1;
    if (a.month != b.month) return (a.month > b.month) ? 1 : -1;
    if (a.day != b.day) return (a.day > b.day) ? 1 : -1;
    if (a.hour != b.hour) return (a.hour > b.hour) ? 1 : -1;
    if (a.minute != b.minute) return (a.minute > b.minute) ? 1 : -1;
    if (a.second != b.second) return (a.second > b.second) ? 1 : -1;
    if (a.id != b.id) return (a.id > b.id) ? 1 : -1;
    return 0;
}

bool IsImage(const CapsAlbumEntry &e)
{
    u8 c = e.file_id.content;
    return c == CapsAlbumFileContents_ScreenShot ||
           c == CapsAlbumFileContents_ExtraScreenShot;
}

static bool IsMovieEntry(const CapsAlbumFileId &fid)
{
    u8 c = fid.content;

    if (c == CapsAlbumFileContents_Movie || c == CapsAlbumFileContents_ExtraMovie) {
        return true;
    }

    return false;
}

const CapsAlbumStorage kStores[2] = { CapsAlbumStorage_Sd, CapsAlbumStorage_Nand };

int RefreshOnce(bool verbose)
{
    static CapsAlbumEntry tmp[kMaxFetch];
    CapsAlbumEntry next[kMaxKeep];
    int nnext = 0;
    for (int s = 0; s < 2; s++) {
        bool mounted = false;
        u64 total = 0;

        capsaIsAlbumMounted(kStores[s], &mounted);
        capsaGetAlbumFileCount(kStores[s], &total);

        if (total == 0)
            continue;
        
        u64 want = total > (u64)kMaxFetch ? (u64)kMaxFetch : total;
        u64 got = 0;

        Result lrc = capsaGetAlbumFileList(kStores[s], &got, tmp, want);
        if (R_FAILED(lrc) || got == 0) {
            continue;
        }

        if (got > want)
            got = want;
        
        for (u64 i = 0; i < got && nnext < kMaxKeep; i++) {
            if (!IsImage(tmp[i]) && !IsMovieEntry(tmp[i].file_id)) {
                continue;
            }
            
            bool dup = false;
            for (int k = 0; k < nnext; k++) {
                if (memcmp(&next[k].file_id, &tmp[i].file_id, sizeof(CapsAlbumFileId)) == 0) {
                    dup = true;
                    break;
                }
            }

            if (!dup)
                next[nnext++] = tmp[i];
        }
    }

    /* Newest first. */
    for (int i = 1; i < nnext; i++) {
        CapsAlbumEntry key = next[i];
        int j = i - 1;
        while (j >= 0 && CmpDate(next[j].file_id.datetime, key.file_id.datetime) < 0) {
            next[j + 1] = next[j];
            j--;
        }
        next[j + 1] = key;
    }

    for (int i = 0; i < nnext; i++)
        s_list[i] = next[i];
    s_count = nnext;
    return s_count;
}

} // namespace

void Init()
{
    if (s_ok)
        return;
    Result rc = capsaInitialize();
    if (R_SUCCEEDED(rc)) {
        s_ok = true;
    } else {
        s_ok = false;
    }
}

void Exit()
{
    if (s_ok) {
        capsaExit();
        s_ok = false;
    }
    s_count = 0;
}

int Refresh()
{
    if (!s_ok) {
        s_count = 0;
        return 0;
    }
    RefreshOnce(false);
    if (s_count == 0) {
        /* Retry a bit until giving up*/
        for (int s = 0; s < 2; s++) {
            capsaRefreshAlbumCache(kStores[s]);
        }
        RefreshOnce(true);
    }
    return s_count;
}

int Count()
{
    return s_count;
}

int FileId(int index, CapsAlbumFileId *out)
{
    if (index < 0 || index >= s_count || !out)
        return -1;
    *out = s_list[index].file_id;
    return 0;
}

int ThumbSize(int index)
{
    if (!s_ok || index < 0 || index >= s_count)
        return -1;
    u64 sz = 0;
    if (R_FAILED(capsaGetAlbumFileSize(&s_list[index].file_id, &sz)) || sz == 0) {
        return -1;
    }
    if (sz > 0x40000)
        return 0x40000;
    return (int)sz;
}

int Thumb(int index, void *out, unsigned cap)
{
    if (!s_ok || index < 0 || index >= s_count || !out || !cap)
        return -1;
    u64 done = 0;
    Result rc = capsaLoadAlbumFileThumbnail(&s_list[index].file_id, &done, out, cap);
    if (R_FAILED(rc) || done == 0 || done > cap) {
        return -1;
    }
    return (int)done;
}

int ImageSize(int index)
{
    if (!s_ok || index < 0 || index >= s_count)
        return -1;
    u64 sz = 0;
    Result rc = capsaGetAlbumFileSize(&s_list[index].file_id, &sz);
    if (R_FAILED(rc) || sz == 0) {
        return -1;
    }
    if (sz > 0x800000)
        return -1;
    return (int)sz;
}

int Image(int index, void *out, unsigned cap)
{
    if (!s_ok || index < 0 || index >= s_count || !out || !cap)
        return -1;
    u64 sz = 0;
    Result src = capsaGetAlbumFileSize(&s_list[index].file_id, &sz);
    if (R_FAILED(src) || sz == 0 || sz > cap) {
        return -1;
    }
    u64 done = 0;
    Result rc = capsaLoadAlbumFile(&s_list[index].file_id, &done, out, sz);
    if (R_FAILED(rc) || done == 0 || done > cap) {
        return -1;
    }
    return (int)done;
}

int Label(int index, char *out, unsigned cap)
{
    if (index < 0 || index >= s_count || !out || !cap)
        return -1;
    const CapsAlbumFileDateTime &d = s_list[index].file_id.datetime;
    snprintf(out, cap, "%04u/%02u/%02u %02u:%02u", d.year, d.month, d.day, d.hour, d.minute);
    out[cap - 1] = 0;
    return (int)strlen(out);
}


namespace {
struct MovieEntry {
    bool used;
    bool open;
    CapsAlbumFileId fid;
    bool isNand;
    FILE *sd;
    FsFile nand;
    u64 size;
};
static MovieEntry *s_movies = NULL;
static int s_movieCap = 0;
static int s_movieCount = 0;
static FsFileSystem s_nandFs;
static bool s_nandFsOpen = false;
static bool FidEqual(const CapsAlbumFileId &a, const CapsAlbumFileId &b)
{
    if (a.application_id != b.application_id) {
        return false;
    }

    if (a.storage != b.storage) {
        return false;
    }

    if (a.content != b.content) {
        return false;
    }

    const CapsAlbumFileDateTime &x = a.datetime;
    const CapsAlbumFileDateTime &y = b.datetime;

    if (x.year != y.year || x.month != y.month || x.day != y.day) {
        return false;
    }

    if (x.hour != y.hour || x.minute != y.minute || x.second != y.second) {
        return false;
    }

    if (x.id != y.id) {
        return false;
    }

    return true;
}
static bool JoinPath(char *out, unsigned outcap, const char *a, const char *b)
{
    size_t al = strlen(a);
    size_t bl = strlen(b);
    size_t need = al + 1 + bl + 1;

    if (need < al || need > outcap) {
        return false;
    }

    memcpy(out, a, al);
    out[al] = 47;
    memcpy(out + al + 1, b, bl);
    out[al + 1 + bl] = 0;
    return true;
}
static bool FsFind(const CapsAlbumFileId &fid, u64 want, char *out, unsigned outcap)
{
    const CapsAlbumFileDateTime &d = fid.datetime;
    char dir[128];
    snprintf(dir, sizeof(dir), "sdmc:/Nintendo/Album/%04u/%02u/%02u", d.year, d.month, d.day);
    char pre[32];
    snprintf(pre, sizeof(pre), "%04u%02u%02u%02u%02u%02u%02u-", d.year, d.month, d.day, d.hour, d.minute, d.second, d.id);
    size_t pren = strlen(pre);
    DIR *dd = opendir(dir);

    if (!dd) {
        return false;
    }

    bool hit = false;
    struct dirent *e;

    while ((e = readdir(dd)) != NULL) {
        if (strncmp(e->d_name, pre, pren) != 0) {
            continue;
        }

        char path[320];

        if (!JoinPath(path, sizeof(path), dir, e->d_name)) {
            continue;
        }

        struct stat stt;
        memset(&stt, 0, sizeof(stt));

        if (stat(path, &stt) != 0) {
            continue;
        }

        if (!S_ISREG(stt.st_mode) || (u64)stt.st_size != want) {
            continue;
        }

        snprintf(out, outcap, "%s", path);
        out[outcap - 1] = 0;
        hit = true;
        break;
    }

    closedir(dd);

    return hit;
}
static bool NandEnsure(void)
{
    if (s_nandFsOpen) {
        return true;
    }

    memset(&s_nandFs, 0, sizeof(s_nandFs));
    Result rc = fsOpenImageDirectoryFileSystem(&s_nandFs, FsImageDirectoryId_Nand);

    if (R_FAILED(rc)) {
            logging::LogLine("[album] Failed to open nand fs (rc: 0x%X)", (unsigned)rc);
            return false;
        }

    s_nandFsOpen = true;

    return true;
}
static bool NandFind(const CapsAlbumFileDateTime &d, u64 want, char *outpath, unsigned outcap)
{
    char pre[32];
    snprintf(pre, sizeof(pre), "%04u%02u%02u%02u%02u%02u%02u-", d.year, d.month, d.day, d.hour, d.minute, d.second, d.id);
    size_t pren = strlen(pre);

    for (int r = 0; r < 2; r++) {
        char root[128];

        if (r == 0) {
            snprintf(root, sizeof(root), "/Album/%04u/%02u/%02u", d.year, d.month, d.day);
        }
        else {
            snprintf(root, sizeof(root), "/%04u/%02u/%02u", d.year, d.month, d.day);
        }

        FsDir dir;
        memset(&dir, 0, sizeof(dir));

        if (R_FAILED(fsFsOpenDirectory(&s_nandFs, root, (u32)(FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles), &dir))) {
            continue;
        }

        FsDirectoryEntry ents[64];
        bool done = false;

        while (!done) {
            s64 got = 0;

            if (R_FAILED(fsDirRead(&dir, &got, 64, ents)) || got <= 0) {
                break;
            }

            for (s64 i = 0; i < got; i++) {
                if (ents[i].type != FsDirEntryType_File) {
                    continue;
                }

                if (strncmp(ents[i].name, pre, pren) != 0) {
                    continue;
                }

                if ((u64)ents[i].file_size != want) {
                    continue;
                }

                if (!JoinPath(outpath, outcap, root, ents[i].name)) {
                    continue;
                }

                done = true;
                break;
            }
        }

        fsDirClose(&dir);

        if (done) {
            return true;
        }
    }

    return false;
}
}
int IsMovie(int index)
{
    if (index < 0 || index >= s_count) {
        return 0;
    }

    u8 c = s_list[index].file_id.content;

    if (c == CapsAlbumFileContents_Movie || c == CapsAlbumFileContents_ExtraMovie) {
        return 1;
    }

    return 0;
}
int MovieOpen(int index)
{
    if (!s_ok || index < 0 || index >= s_count) {
        logging::LogLine("[album] Failed to open movie (idx: %d count: %d)", index, s_count);
        return -1;
    }

    const CapsAlbumFileId &fid = s_list[index].file_id;

    int reuse = -1;

    for (int i = 0; i < s_movieCount; i++) {
        if (!s_movies[i].used || !FidEqual(s_movies[i].fid, fid)) {
            continue;
        }

        if (s_movies[i].open) {
            return i + 1;
        }

        reuse = i;
    }

    u64 fsz = 0;
    Result szrc = capsaGetAlbumFileSize(&fid, &fsz);

    if (R_FAILED(szrc) || fsz == 0) {
        logging::LogLine("[album] Failed to size movie (idx: %d rc: 0x%X)", index, (unsigned)szrc);
        return -1;
    }

    int slot = reuse;

    if (slot < 0) {
        if (s_movieCount >= s_movieCap) {
            int ncap = s_movieCap == 0 ? 4 : s_movieCap * 2;
            MovieEntry *nn = (MovieEntry *)realloc(s_movies, (size_t)ncap * sizeof(MovieEntry));

            if (!nn) {
                logging::LogLine("[album] Failed to grow movie cache");
                return -1;
            }

            memset(nn + s_movieCap, 0, (size_t)(ncap - s_movieCap) * sizeof(MovieEntry));
            s_movies = nn;
            s_movieCap = ncap;
        }

        slot = s_movieCount;
        s_movieCount++;
    }

    MovieEntry &e = s_movies[slot];
    memset(&e, 0, sizeof(e));
    e.fid = fid;
    e.size = fsz;

    if (fid.storage == CapsAlbumStorage_Nand) {
        if (!NandEnsure()) {
            return -1;
        }

        char npath[320];
        npath[0] = 0;

        if (!NandFind(fid.datetime, fsz, npath, sizeof(npath))) {
            logging::LogLine("[album] Failed to find movie (idx: %d store: %u)", index, (unsigned)fid.storage);
            return -1;
        }

        Result frc = fsFsOpenFile(&s_nandFs, npath, FsOpenMode_Read, &e.nand);

        if (R_FAILED(frc)) {
            for (int i = 0; i < s_movieCount; i++) {
                if (s_movies[i].used && s_movies[i].open && s_movies[i].isNand) {
                    fsFileClose(&s_movies[i].nand);
                    s_movies[i].open = false;
                }
            }

            for (int rt = 0; rt < 3 && R_FAILED(frc); rt++) {
                svcSleepThread(200000000ull);
                frc = fsFsOpenFile(&s_nandFs, npath, FsOpenMode_Read, &e.nand);
            }

            if (R_FAILED(frc)) {
                logging::LogLine("[album] Failed to open nand movie (idx: %d rc: 0x%X)", index, (unsigned)frc);
                return -1;
            }
        }

        e.isNand = true;
    }
    else {
        char path[320];
        path[0] = 0;

        if (!FsFind(fid, fsz, path, sizeof(path))) {
            logging::LogLine("[album] Failed to find movie (idx: %d store: %u)", index, (unsigned)fid.storage);
            return -1;
        }

        FILE *ff = fopen(path, "rb");

        if (!ff) {
            logging::LogLine("[album] Failed to fopen movie (idx: %d)", index);
            return -1;
        }

        e.sd = ff;
        e.isNand = false;
    }

    e.used = true;
    e.open = true;

    return slot + 1;
}
u64 MovieSize(int h)
{
    if (h <= 0 || h > s_movieCount) {
        return 0;
    }

    MovieEntry &e = s_movies[h - 1];

    if (!e.used || !e.open) {
        return 0;
    }

    return e.size;
}
int MovieRead(int h, u64 off, void *out, unsigned cap)
{
    if (h <= 0 || h > s_movieCount || !out || !cap) {
        return -1;
    }

    MovieEntry &e = s_movies[h - 1];

    if (!e.used || !e.open || off >= e.size) {
        return -1;
    }

    if (e.isNand) {
        u64 br = 0;

        if (R_FAILED(fsFileRead(&e.nand, (s64)off, out, cap, 0, &br)) || br == 0) {
            return -1;
        }

        return (int)br;
    }

    if (fseeko(e.sd, (off_t)off, SEEK_SET) != 0) {
        return -1;
    }

    size_t n = fread(out, 1, cap, e.sd);

    if (n == 0) {
        return -1;
    }

    return (int)n;
}
int MovieClose(int h)
{
    if (h <= 0 || h > s_movieCount) {
        return -1;
    }

    MovieEntry &e = s_movies[h - 1];

    if (!e.used || !e.open) {
        return -1;
    }

    return 0;
}
} // namespace album
