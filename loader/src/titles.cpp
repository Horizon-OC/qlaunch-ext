/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "titles.hpp"
#include <switch.h>
#include <cstring>
#include <shared/logging.hpp>

namespace titles {

namespace {

constexpr int kMaxTitles = 128;
constexpr int kChunkCount = 30;

struct Entry {
    u64 tid = 0;
    char name[0x201] = {0};
    bool gamecard = false;
    bool ejected = false;
};

Entry s_list[kMaxTitles];
int s_count = 0;
static Event s_ev;
static bool s_evInit = false;

/* NsApplicationControlData is NACP + 128KB icon */
NsApplicationControlData s_ctrl;

bool ResolveName(u64 tid, char *out, unsigned cap)
{
    if (cap == 0)
        return false;
    out[0] = 0;
    u64 actual = 0;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, tid,
                                            &s_ctrl, sizeof(s_ctrl), &actual);
    if (R_FAILED(rc) || actual < sizeof(NacpStruct))
        return false;
    NacpLanguageEntry *lang = nullptr;
    if (R_SUCCEEDED(nacpGetLanguageEntry(&s_ctrl.nacp, &lang)) && lang && lang->name[0])
        snprintf(out, cap, "%s", lang->name);
    else {
        for (int i = 0; i < 16 && !out[0]; i++) {
            if (s_ctrl.nacp.lang[i].name[0])
                snprintf(out, cap, "%s", s_ctrl.nacp.lang[i].name);
        }
    }
    out[cap - 1] = 0;
    return out[0] != 0;
}

int FetchIcon(u64 tid, unsigned *out_size)
{
    u64 actual = 0;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, tid,
                                            &s_ctrl, sizeof(s_ctrl), &actual);
    if (R_FAILED(rc) || actual <= sizeof(NacpStruct))
        return -1;
    u64 bytes = actual - sizeof(NacpStruct);
    if (bytes > sizeof(s_ctrl.icon))
        bytes = sizeof(s_ctrl.icon);
    if (bytes == 0)
        return -1;
    if (out_size)
        *out_size = (unsigned)bytes;
    return 0;
}

} // namespace

bool IsOnGameCard(u64 tid)
{
    NsApplicationContentMetaStatus st{};
    s32 out = 0;
    if (R_FAILED(nsListApplicationContentMetaStatus(tid, 0, &st, 1, &out)))
        return false;
    if (out <= 0)
        return false;
    return st.storageID == NcmStorageId_GameCard;
}
int Refresh()
{
    /* Collect titles that are currently loaded first */
    u64 tids[kMaxTitles];
    int ntid = 0;
    NsApplicationRecord chunk[kChunkCount] = {};
    s32 offset = 0;
    while (offset < kMaxTitles && ntid < kMaxTitles) {
        s32 got = 0;
        Result rc = nsListApplicationRecord(chunk, kChunkCount, offset, &got);
        if (R_FAILED(rc)) {
            logging::LogLine("[titles] list rc=0x%X", rc);
            break;
        }
        if (got <= 0)
            break;
        for (s32 i = 0; i < got && ntid < kMaxTitles; i++) {
            u64 tid = chunk[i].application_id;
            if (!tid)
                continue;
            bool dup = false;
            for (int j = 0; j < ntid; j++) {
                if (tids[j] == tid) {
                    dup = true;
                    break;
                }
            }
            if (!dup)
                tids[ntid++] = tid;
        }
        offset += got;
        if (got < kChunkCount)
            break;
    }

    Entry next[kMaxTitles];
    int nnext = 0;
    bool changed = (ntid != s_count);
    for (int i = 0; i < ntid; i++) {
        bool kept = false;
        for (int j = 0; j < s_count; j++) {
            if (s_list[j].tid == tids[i]) {
                next[nnext++] = s_list[j];
            if (next[nnext - 1].ejected) changed = true;
            next[nnext - 1].ejected = false;
                kept = true;
                break;
            }
        }
        if (kept)
            continue;
        changed = true;
        next[nnext].tid = tids[i];
            next[nnext].gamecard = IsOnGameCard(tids[i]);
            next[nnext].ejected = false;
        if (!ResolveName(tids[i], next[nnext].name, sizeof(next[nnext].name)))
            snprintf(next[nnext].name, sizeof(next[nnext].name),
                     "%016llX", (unsigned long long)tids[i]);
        nnext++;
    }
    for (int j = 0; j < s_count && nnext < kMaxTitles; j++) {
        bool live = false;
        for (int i = 0; i < ntid; i++) {
            if (tids[i] == s_list[j].tid) {
                live = true;
                break;
            }
        }
        if (live || !s_list[j].gamecard)
            continue;
        next[nnext] = s_list[j];
        if (!next[nnext].ejected) {
            next[nnext].ejected = true;
            changed = true;
        }
        nnext++;
    }
    /* ascending order. */
    for (int i = 1; i < nnext; i++) {
        Entry key = next[i];
        int j = i - 1;
        while (j >= 0 && next[j].tid > key.tid) {
            next[j + 1] = next[j];
            j--;
        }
        next[j + 1] = key;
    }
    for (int i = 0; i < nnext; i++)
        s_list[i] = next[i];
    s_count = nnext;
    u64 uids[kMaxTitles];
    for (int i = 0; i < s_count; i++)
        uids[i] = s_list[i].tid;
    NsApplicationView views[kMaxTitles];
    if (s_count > 0 && R_SUCCEEDED(nsGetApplicationView(views, uids, s_count))) {
        for (int i = 0; i < s_count; i++) {
            if (views[i].flags & BIT(6))
                s_list[i].gamecard = true;
            if (s_list[i].gamecard) {
                bool ej = !(views[i].flags & BIT(7));
                if (ej != s_list[i].ejected) {
                    s_list[i].ejected = ej;
                    changed = true;
                }
            }
        }
    }
    if (changed)
        logging::LogLine("[titles] refreshed: %d", s_count);
    return s_count;
}

int Count()
{
    return s_count;
}

u64 Id(int index)
{
    if (index < 0 || index >= s_count)
        return 0;
    return s_list[index].tid;
}
int Ejected(int index)
{
    if (index < 0 || index >= s_count)
        return 0;
    return s_list[index].ejected ? 1 : 0;
}
int TitlesChanged()
{
    if (!s_evInit) {
        if (R_FAILED(nsGetApplicationRecordUpdateSystemEvent(&s_ev)))
            return 0;
        s_evInit = true;
    }
    if (R_SUCCEEDED(eventWait(&s_ev, 0))) {
        svcSleepThread(100000000ULL);
        return 1;
    }
    return 0;
}

int Name(int index, char *out, unsigned out_cap)
{
    if (index < 0 || index >= s_count || !out || !out_cap)
        return -1;
    snprintf(out, out_cap, "%s", s_list[index].name);
    out[out_cap - 1] = 0;
    return (int)strlen(out);
}

int IconSize(int index)
{
    if (index < 0 || index >= s_count)
        return -1;
    unsigned size = 0;
    if (FetchIcon(s_list[index].tid, &size) < 0)
        return -1;
    return (int)size;
}

int Icon(int index, void *out, unsigned cap)
{
    if (index < 0 || index >= s_count || !out || !cap)
        return -1;
    unsigned size = 0;
    if (FetchIcon(s_list[index].tid, &size) < 0)
        return -1;
    if (size > cap)
        return -1;
    memcpy(out, s_ctrl.icon, size);
    return (int)size;
}

} // namespace titles
