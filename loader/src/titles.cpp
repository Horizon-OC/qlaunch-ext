/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "titles.hpp"
#include <switch.h>
#include <cstring>
#include <shared/logging.hpp>
#include <stdio.h>

namespace titles {

namespace {

constexpr int kMaxTitles = 128;
constexpr int kChunkCount = 30;
constexpr char kOrderPath[] = "sdmc:/qlaunch-ext/order.json";

struct Entry {
    u64 tid = 0;
    char name[0x201] = {0};
    bool gamecard = false;
    bool ejected = false;
    u64 seq = 0;
};

Entry s_list[kMaxTitles];
int s_count = 0;
static Event s_ev;
static bool s_evInit = false;
static u64 s_seq = 0;
static bool s_orderDirty = false;
static u64 s_fileTids[kMaxTitles];
static int s_fileN = -1;
static char s_wb[] = { 'w', 'b', 0 };
static char s_rb[] = { 'r', 'b', 0 };

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


static void WriteU64(FILE *f, u64 v)
{
    char b[24];
    int n = 0;

    if (v == 0)
        fputc('0', f);
    
    while (v > 0 && n < 24) {
        b[n++] = (char)('0' + v % 10);
        v /= 10;
    }

    while (n > 0)
        fputc(b[--n], f);
}

static int ParseOrderFile(u64 *tids, int cap)
{
    int n = 0;
    FILE *f = fopen(kOrderPath, s_rb);
    if (!f)
        return 0;
    
    int c;
    while ((c = fgetc(f)) != -1 && n < cap) {
        if (c != 'g')
            continue;
        
        if (fgetc(f) != 'a' || fgetc(f) != 'm' || fgetc(f) != 'e')
            continue;
        
        while ((c = fgetc(f)) != -1 && c >= '0' && c <= '9')
            ;
        
        while (c != -1 && !(c >= '0' && c <= '9'))
            c = fgetc(f);
        
        u64 tid = 0;
        bool any = false;
        while (c != -1 && c >= '0' && c <= '9') {
            any = true;
            tid = tid * 10 + (u64)(c - '0');
            c = fgetc(f);
        }

        if (any && tid != 0)
            tids[n++] = tid;
    }
    fclose(f);
    return n;
}

static void SaveOrder()
{
    FILE *f = fopen(kOrderPath, s_wb);
    if (!f)
        return;
    
    fputc('{', f);
    fputc('\n', f);
    for (int i = 0; i < s_count; i++) {
        fputs("\"game", f);
        WriteU64(f, (u64)(i + 1));
        fputs("\": ", f);
        WriteU64(f, s_list[i].tid);
        if (i + 1 < s_count)
            fputc(',', f);
        fputc('\n', f);
    }

    fputc('}', f);
    fputc('\n', f);
    fclose(f);
}
static void SortByRank(Entry *a, int n)
{
    for (int i = 1; i < n; i++) {
        Entry key = a[i];
        int j = i - 1;
        while (j >= 0 && (a[j].seq < key.seq || (a[j].seq == key.seq && a[j].tid > key.tid))) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}
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
                kept = true;
                break;
            }
        }
        if (kept)
            continue;
        changed = true;
        next[nnext].tid = tids[i];
        next[nnext].gamecard = IsOnGameCard(tids[i]);
        next[nnext].seq = ++s_seq;
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
    if (s_fileN < 0) {
        s_fileN = ParseOrderFile(s_fileTids, kMaxTitles);
        logging::LogLine("[titles] order file: %d", s_fileN);
        for (int k = 0; k < s_fileN; k++) {
            for (int i = 0; i < nnext; i++) {
                if (next[i].tid == s_fileTids[k]) {
                    if (next[i].seq < (u64)(s_fileN - k) + (u64)nnext)
                        next[i].seq = (u64)(s_fileN - k) + (u64)nnext;
                }
            }
        }
        if (s_fileN > 0 && s_seq < (u64)s_fileN + (u64)nnext)
            s_seq = (u64)s_fileN + (u64)nnext;
    }
    SortByRank(next, nnext);
    for (int i = 0; i < nnext; i++)
        s_list[i] = next[i];
    s_count = nnext;
    u64 uids[kMaxTitles];
    for (int i = 0; i < s_count; i++)
        uids[i] = s_list[i].tid;
    
    bool prevEj[kMaxTitles];
    for (int i = 0; i < s_count; i++) {
        prevEj[i] = s_list[i].ejected;
    }

    NsApplicationView views[kMaxTitles];
    bool resorted = false;
    if (s_count > 0 && R_SUCCEEDED(nsGetApplicationView(views, uids, s_count))) {
        for (int i = 0; i < s_count; i++) {
            if (views[i].flags & BIT(6))
                s_list[i].gamecard = true;
            if (s_list[i].gamecard) {
                bool ej = !(views[i].flags & BIT(7));
                if (ej != s_list[i].ejected) {
                    s_list[i].ejected = ej;

                    if (!ej && prevEj[i]) {
                        s_list[i].seq = ++s_seq;
                        resorted = true;
                    }
                    changed = true;
                }
            }
        }
    }
    if (resorted) {
        SortByRank(s_list, s_count);
        changed = true;
    }
    if (changed || s_orderDirty) {
        SaveOrder();
        s_orderDirty = false;
    }
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
void NoteLaunched(u64 tid)
{
    int idx = -1;
    for (int i = 0; i < s_count; i++) {
        if (s_list[i].tid == tid) { idx = i; break; }
    }

    if (idx < 0) 
        return;

    Entry e = s_list[idx];
    e.seq = ++s_seq;
    for (int i = idx; i > 0; --i) {
        s_list[i] = s_list[i - 1];
    }
    s_list[0] = e;

    SaveOrder();
    s_orderDirty = false;
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
