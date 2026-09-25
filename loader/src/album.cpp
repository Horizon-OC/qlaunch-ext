/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "album.hpp"
#include <cstring>
#include <cstdio>
#include <shared/logging.hpp>

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
            if (!IsImage(tmp[i]))
                continue;
            
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

} // namespace album