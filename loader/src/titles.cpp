/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "titles.hpp"
#include "folders.hpp"
#include "plugins/vtitles.hpp"
#include <switch.h>
#include <cstring>
#include <shared/logging.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

namespace titles {

const char kOrderPath[] = "sdmc:/qlaunch-ext/order.json";

namespace {

constexpr int kMaxTitles = 256;
constexpr int kChunkCount = 30;
constexpr int kOrderFileCap = 65536;

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

/* For newer games using compressed NACP */

/// @brief Decompress FW21+ title data into the language table.
/// @param nacp Control NACP to convert in place.
/// @return True when entries are usable (or were already plain).
static bool NacpConvertTitleData(NacpStruct *nacp) {
    if (!nacp)
        return false;

    if (nacp->titles_data_format == 0)
        return true;

    if (nacp->titles_data_format != 1) {
        logging::LogLine("[titles] Cant decode NACP (format %u)", (unsigned)nacp->titles_data_format);
        return false;
    }

    u16 csize = nacp->lang_data.compressed_data.buffer_size;

    if (csize == 0 || csize > sizeof(nacp->lang_data.compressed_data.buffer)) {
        logging::LogLine("[titles] Bad NACP payload (%u)", (unsigned)csize);
        return false;
    }

    NacpLanguageEntry tmp[32];
    z_stream stream = {};

    stream.avail_out = sizeof(tmp);
    stream.next_out = (Bytef *)tmp;
    stream.avail_in = csize;
    stream.next_in = nacp->lang_data.compressed_data.buffer;

    int ret = inflateInit2(&stream, -15);
    if (ret != Z_OK) {
        logging::LogLine("[titles] Can't inflate NACP (rc: %d)", ret);
        return false;
    }

    ret = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (ret != Z_STREAM_END) {
        logging::LogLine("[titles] Can't inflate NACP (rc: %d)", ret);
        return false;
    }
    nacp->titles_data_format = 0;
    memcpy(nacp->lang_data.lang, tmp, sizeof(nacp->lang_data.lang));
    return true;
}

/// @brief Resolve a title's display name from its control NACP.
/// @param tid Title id.
/// @param out Output buffer.
/// @param cap Buffer capacity.
/// @return True when a non-empty name was written.
bool ResolveName(u64 tid, char *out, unsigned cap) {
    if (cap == 0)
        return false;
    out[0] = 0;
    u64 actual = 0;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, tid,
                                            &s_ctrl, sizeof(s_ctrl), &actual);
    if (R_FAILED(rc) || actual < sizeof(NacpStruct))
        return false;

    NacpConvertTitleData(&s_ctrl.nacp);

    NacpLanguageEntry *lang = nullptr;
    if (R_SUCCEEDED(nacpGetLanguageEntry(&s_ctrl.nacp, &lang)) && lang && lang->name[0])
        snprintf(out, cap, "%s", lang->name);
    else {
        for (int i = 0; i < 16 && !out[0]; i++) {
            if (s_ctrl.nacp.lang_data.lang[i].name[0])
                snprintf(out, cap, "%s", s_ctrl.nacp.lang_data.lang[i].name);
        }
    }
    out[cap - 1] = 0;
    return out[0] != 0;
}

/// @brief Resolve a title's display version from its control NACP.
/// @param tid Title id.
/// @param out Output buffer.
/// @param cap Buffer capacity.
/// @return True when a non-empty version was written.
bool ResolveVersion(u64 tid, char *out, unsigned cap) {
    if (cap == 0)
        return false;
    out[0] = 0;
    u64 actual = 0;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, tid,
                                            &s_ctrl, sizeof(s_ctrl), &actual);
    if (R_FAILED(rc) || actual < sizeof(NacpStruct))
        return false;

    char version[0x10];
    memcpy(version, s_ctrl.nacp.display_version, sizeof(version));
    version[sizeof(version) - 1] = 0;
    if (version[0] == 0)
        return false;
    snprintf(out, cap, "%s", version);
    out[cap - 1] = 0;
    return out[0] != 0;
}

/// @brief Resolve a title's publisher from its control NACP author.
/// @param tid Title id.
/// @param out Output buffer.
/// @param cap Buffer capacity.
/// @return True when a non-empty publisher was written.
bool ResolvePublisher(u64 tid, char *out, unsigned cap) {
    if (cap == 0)
        return false;
    out[0] = 0;
    u64 actual = 0;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, tid,
                                            &s_ctrl, sizeof(s_ctrl), &actual);
    if (R_FAILED(rc) || actual < sizeof(NacpStruct))
        return false;

    NacpConvertTitleData(&s_ctrl.nacp);

    NacpLanguageEntry *lang = nullptr;
    if (R_SUCCEEDED(nacpGetLanguageEntry(&s_ctrl.nacp, &lang)) && lang && lang->author[0])
        snprintf(out, cap, "%s", lang->author);
    else {
        for (int i = 0; i < 16 && !out[0]; i++) {
            if (s_ctrl.nacp.lang_data.lang[i].author[0])
                snprintf(out, cap, "%s", s_ctrl.nacp.lang_data.lang[i].author);
        }
    }
    out[cap - 1] = 0;
    return out[0] != 0;
}

int FetchIcon(u64 tid, unsigned *out_size) {
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

/* SD Icon overrides */
constexpr char kIconDir[] = "sdmc:/qlaunch-ext/icons";
constexpr unsigned kIconFileCap = 0x40000;

/// @brief Build the override path for a title id.
/// @param tid Title id (real or virtual).
/// @param out Output buffer.
/// @param cap Buffer capacity.
/// @return True when the path fit.
static bool IconFilePath(u64 tid, char *out, unsigned cap) {
    if (!out || cap < 64)
        return false;
    int w = snprintf(out, cap, "%s/%016llX.jpg", kIconDir, (unsigned long long)tid);
    return w > 0 && (unsigned)w < cap;
}

/// @brief Give an override icon's byte size.
/// @param tid Title id (real or virtual).
/// @return Byte size (<0 if none or unusable).
static int IconFileSize(u64 tid) {
    char path[80];
    if (!IconFilePath(tid, path, sizeof(path)))
        return -1;
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    if (sz <= 2 || (unsigned)sz > kIconFileCap)
        return -1;
    return (int)sz;
}

/// @brief Copy an override icon's bytes.
/// @param tid Title id (real or virtual).
/// @param out Output buffer.
/// @param cap Buffer capacity.
/// @return Bytes written (<0 on error).
static int IconFileRead(u64 tid, void *out, unsigned cap) {
    int want = IconFileSize(tid);
    if (want < 0 || !out || (unsigned)want > cap)
        return -1;
    char path[80];
    if (!IconFilePath(tid, path, sizeof(path)))
        return -1;
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    size_t got = fread(out, 1, (unsigned)want, f);
    fclose(f);
    if (got != (unsigned)want)
        return -1;
    return (int)got;
}

} // namespace

static void WriteU64(FILE *f, u64 v) {
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

static bool IsSpaceByte(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/// @brief Locate a top-level "folders" object span so game scanning skips it.
/// Folder names are free user text and may contain game-like patterns.
/// @param text File text.
/// @param len Text length.
/// @param spanStartOut Span start output (key quote).
/// @param spanEndOut Span end output (past closing brace).
/// @return True when a folders object was found.
static bool FindFoldersSpan(const char *text, size_t len, size_t *spanStartOut, size_t *spanEndOut) {
    static const char keyText[] = "folders";
    size_t i = 0;
    int depth = 0;
    while (i < len) {
        char c = text[i];
        if (c == '"') {
            size_t keyStart = i++;
            size_t k = 0;
            while (k < 7 && i < len && text[i] == keyText[k]) {
                i++;
                k++;
            }
            if (k == 7 && i < len && text[i] == '"' && depth == 1) {
                i++;
                while (i < len && IsSpaceByte(text[i]))
                    i++;
                if (i < len && text[i] == ':') {
                    i++;
                    while (i < len && IsSpaceByte(text[i]))
                        i++;
                    if (i < len && text[i] == '{') {
                        int inner = 0;
                        while (i < len) {
                            char d = text[i];
                            if (d == '"') {
                                i++;
                                while (i < len) {
                                    if (text[i] == '\\')
                                        i += 2;
                                    else if (text[i] == '"') {
                                        i++;
                                        break;
                                    } else {
                                        i++;
                                    }
                                }
                                continue;
                            }
                            if (d == '{')
                                inner++;
                            else if (d == '}') {
                                inner--;
                                i++;
                                if (inner == 0)
                                    break;
                                continue;
                            }
                            i++;
                        }
                        if (inner == 0) {
                            *spanStartOut = keyStart;
                            *spanEndOut = i;
                            return true;
                        }
                        return false;
                    }
                }
            }
            continue;
        }
        if (c == '{')
            depth++;
        else if (c == '}')
            depth--;
        i++;
    }
    return false;
}

/// @brief Scan a buffer range for game entries (original stream semantics).
/// @param p Range start.
/// @param end Range end.
/// @param tids Output tids.
/// @param n Output count (updated).
/// @param cap Output capacity.
static void ScanGames(const char *p, const char *end, u64 *tids, int &n, int cap) {
    size_t i = 0;
    size_t len = (size_t)(end - p);
    while (i < len && n < cap) {
        if (p[i] != 'g') {
            i++;
            continue;
        }
        if (i + 3 >= len || p[i + 1] != 'a' || p[i + 2] != 'm' || p[i + 3] != 'e') {
            i++;
            continue;
        }
        i += 4;
        while (i < len && p[i] >= '0' && p[i] <= '9')
            i++;
        while (i < len && !(p[i] >= '0' && p[i] <= '9'))
            i++;
        u64 tid = 0;
        bool any = false;
        while (i < len && p[i] >= '0' && p[i] <= '9') {
            any = true;
            tid = tid * 10 + (u64)(p[i] - '0');
            i++;
        }
        if (any && tid != 0)
            tids[n++] = tid;
    }
}

static int ParseOrderFile(u64 *tids, int cap) {
    int n = 0;
    FILE *f = fopen(kOrderPath, s_rb);
    if (!f)
        return 0;
    char *text = (char *)malloc(kOrderFileCap);
    if (!text) {
        fclose(f);
        return 0;
    }
    size_t got = fread(text, 1, kOrderFileCap - 1, f);
    fclose(f);
    size_t spanStart = 0;
    size_t spanEnd = 0;
    if (FindFoldersSpan(text, got, &spanStart, &spanEnd)) {
        ScanGames(text, text + spanStart, tids, n, cap);
        ScanGames(text + spanEnd, text + got, tids, n, cap);
    } else {
        ScanGames(text, text + got, tids, n, cap);
    }
    free(text);
    return n;
}

/// @brief Persist game order plus the folders section.
/// Single writer for order.json; folders mutators funnel through here.
void Save() {
    FILE *f = fopen(kOrderPath, s_wb);
    if (!f)
        return;

    fputc('{', f);
    fputc('\n', f);
    bool first = true;
    for (int i = 0; i < s_count; i++) {
        if (!first)
            fputc(',', f);
        first = false;
        fputs("\"game", f);
        WriteU64(f, (u64)(i + 1));
        fputs("\": ", f);
        WriteU64(f, s_list[i].tid);
        fputc('\n', f);
    }
    if (!first)
        fputc(',', f);
    fputc('\n', f);
    folders::WriteSection(f);
    fputc('\n', f);
    fputc('}', f);
    fputc('\n', f);
    fclose(f);
}

static void SortByRank(Entry *a, int n) {
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

bool IsOnGameCard(u64 tid) {
    NsApplicationContentMetaStatus st{};
    s32 out = 0;
    if (R_FAILED(nsListApplicationContentMetaStatus(tid, 0, &st, 1, &out)))
        return false;
    if (out <= 0)
        return false;
    return st.storageID == NcmStorageId_GameCard;
}

int Refresh() {
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
    int prevTotal = s_count;
    bool changed = false;
    /* A failed enumeration is not a removal: keep the old list instead
       of gutting order.json on transient ns errors. */
    if (ntid == 0 && prevTotal > 0) {
        logging::LogLine("[titles] empty enumeration, keeping %d", prevTotal);
        return prevTotal;
    }
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

    /* Merge virtual titles into the title list */
    vtitles::Refresh();
    for (int vi = 0, nv = vtitles::Count(); vi < nv && nnext < kMaxTitles; vi++) {
        u64 vtid = vtitles::Id(vi);
        if (!vtid)
            continue;
        bool kept = false;
        for (int j = 0; j < s_count; j++) {
            if (s_list[j].tid == vtid) {
                next[nnext] = s_list[j];
                kept = true;
                break;
            }
        }
        char vname[sizeof(next[nnext].name)];
        int namerc = vtitles::Name(vi, vname, sizeof(vname));
        if (namerc < 0) {
            if (!kept)
                continue; /* skip nameless provider entries */
            memcpy(vname, next[nnext].name, sizeof(vname));
        }
        if (!kept) {
            changed = true;
            next[nnext].tid = vtid;
            next[nnext].gamecard = false;
            next[nnext].seq = ++s_seq;
            next[nnext].ejected = false;
        }
        memcpy(next[nnext].name, vname, sizeof(vname));
        nnext++;
    }
    /* Drop folder members that no longer exist (deleted titles, ejected
       cards already handled above, uninstalled forwarders). */
    {
        u64 liveTids[kMaxTitles];
        int liveCount = 0;
        for (int i = 0; i < nnext && liveCount < kMaxTitles; i++)
            liveTids[liveCount++] = next[i].tid;
        folders::Prune(liveTids, liveCount);
    }
    if (nnext != prevTotal)
        changed = true;
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

    for (int k = 0; k < s_fileN; k++) {
        bool live = false;
        for (int i = 0; i < nnext; i++) {
            if (next[i].tid == s_fileTids[k]) {
                live = true;
                break;
            }
        }
        if (!live) {
            changed = true;
            break;
        }
    }

    SortByRank(next, nnext);

    for (int i = 0; i < nnext; i++) {
        s_list[i] = next[i];
    }

    s_count = nnext;
    /* Virtual titles don't have real IDs. */
    u64 uids[kMaxTitles];
    int vmap[kMaxTitles];
    int nuid = 0;
    for (int i = 0; i < s_count; i++) {
        if (vtitles::IsId(s_list[i].tid))
            continue;
        vmap[nuid] = i;
        uids[nuid++] = s_list[i].tid;
    }

    bool prevEj[kMaxTitles];
    for (int i = 0; i < s_count; i++) {
        prevEj[i] = s_list[i].ejected;
    }

    NsApplicationView views[kMaxTitles];
    bool resorted = false;
    if (nuid > 0 && R_SUCCEEDED(nsGetApplicationView(views, uids, nuid))) {
        for (int k = 0; k < nuid; k++) {
            int i = vmap[k];
            if (views[k].flags & BIT(6))
                s_list[i].gamecard = true;
            if (s_list[i].gamecard) {
                bool ej = !(views[k].flags & BIT(7));
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
        Save();
        s_orderDirty = false;
        /* Memory mirrors the rebuilt file, so gutted tids refire once. */
        s_fileN = 0;
        for (int i = 0; i < s_count && i < kMaxTitles; i++)
            s_fileTids[s_fileN++] = s_list[i].tid;
    }
    return s_count;
}

int Count() {
    return s_count;
}

u64 Id(int index) {
    if (index < 0 || index >= s_count)
        return 0;
    return s_list[index].tid;
}

int Ejected(int index) {
    if (index < 0 || index >= s_count)
        return 0;
    return s_list[index].ejected ? 1 : 0;
}

void NoteLaunched(u64 tid) {
    int idx = -1;
    for (int i = 0; i < s_count; i++) {
        if (s_list[i].tid == tid) {
            idx = i;
            break;
        }
    }

    if (idx < 0)
        return;

    Entry e = s_list[idx];
    e.seq = ++s_seq;
    for (int i = idx; i > 0; --i) {
        s_list[i] = s_list[i - 1];
    }
    s_list[0] = e;

    Save();
    s_orderDirty = false;
}

int TitlesChanged() {
    /* Virtual-set changes (provider load/unload, recount) trip first. */
    if (vtitles::Changed())
        return 1;
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

int Name(int index, char *out, unsigned out_cap) {
    if (index < 0 || index >= s_count || !out || !out_cap)
        return -1;
    int vi = vtitles::IndexOf(s_list[index].tid);
    if (vi >= 0)
        return vtitles::Name(vi, out, out_cap);
    snprintf(out, out_cap, "%s", s_list[index].name);
    out[out_cap - 1] = 0;
    return (int)strlen(out);
}

int Version(int index, char *out, unsigned out_cap) {
    if (index < 0 || index >= s_count || !out || !out_cap)
        return -1;
    u64 tid = s_list[index].tid;
    int vi = vtitles::IndexOf(tid);
    if (vi >= 0)
        return vtitles::Version(vi, out, out_cap);
    if (!ResolveVersion(tid, out, out_cap))
        return -1;
    return (int)strlen(out);
}

int Publisher(int index, char *out, unsigned out_cap) {
    if (index < 0 || index >= s_count || !out || !out_cap)
        return -1;
    u64 tid = s_list[index].tid;
    int vi = vtitles::IndexOf(tid);
    if (vi >= 0)
        return vtitles::Publisher(vi, out, out_cap);
    if (!ResolvePublisher(tid, out, out_cap))
        return -1;
    return (int)strlen(out);
}

int IconSize(int index) {
    if (index < 0 || index >= s_count)
        return -1;
    u64 tid = s_list[index].tid;
    int vi = vtitles::IndexOf(tid);
    if (vi >= 0) {
        int vsz = vtitles::IconSize(vi);
        if (vsz >= 0)
            return vsz;
        return IconFileSize(tid);
    }
    unsigned size = 0;
    if (FetchIcon(tid, &size) < 0)
        return IconFileSize(tid);
    return (int)size;
}

int Icon(int index, void *out, unsigned cap) {
    if (index < 0 || index >= s_count || !out || !cap)
        return -1;
    u64 tid = s_list[index].tid;
    int vi = vtitles::IndexOf(tid);
    if (vi >= 0) {
        int got = vtitles::Icon(vi, out, cap);
        if (got >= 0)
            return got;
        return IconFileRead(tid, out, cap);
    }
    unsigned size = 0;
    if (FetchIcon(tid, &size) < 0)
        return IconFileRead(tid, out, cap);
    if (size > cap)
        return -1;
    memcpy(out, s_ctrl.icon, size);
    return (int)size;
}

} // namespace titles
