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
        int sAlbumRefreshCount = 0;
        bool sInitialized = false;

        /// @brief Compare album file dates
        /// @param a First one to compare
        /// @param b Second one to compare
        /// @return less than, equal or greater
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
        
        /// @brief Is an entry a image
        /// @param e A entry
        /// @return true or false
        bool IsImage(const CapsAlbumEntry &e) {
            u8 c = e.file_id.content;
            return c == CapsAlbumFileContents_ScreenShot || c == CapsAlbumFileContents_ExtraScreenShot;
        }

        /// @brief Is the entry a movie
        /// @param fid A album file ID
        /// @return true or false
        static bool IsMovieEntry(const CapsAlbumFileId &fid) {
            u8 c = fid.content;

            if (c == CapsAlbumFileContents_Movie || c == CapsAlbumFileContents_ExtraMovie) {
                return true;
            }

            return false;
        }
        
        /// @brief For array indexing
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
            sAlbumRefreshCount = nnext;
            return sAlbumRefreshCount;
        }

    } // namespace
    
    /// @brief Initialize the capsrv
    void Init() {
        if (sInitialized)
            return;

        Result rc = capsaInitialize();

        sInitialized = R_SUCCEEDED(rc);
    }

    /// @brief Exit capsrv
    void Exit()
    {
        if (sInitialized) {
            capsaExit();
            sInitialized = false;
        }
        sAlbumRefreshCount = 0;
    }

    /// @brief Refresh the album
    /// @return The amount of images refreshed.
    int Refresh()
    {
        if (!sInitialized) {
            sAlbumRefreshCount = 0;
            return 0;
        }

        RefreshOnce(false);
        if (sAlbumRefreshCount == 0) {
            /* Retry a bit until giving up*/
            for (int s = 0; s < 2; s++) {
                capsaRefreshAlbumCache(kStores[s]);
            }
            RefreshOnce(true);
        }
        return sAlbumRefreshCount;
    }

    /// @brief Give the current count of images in the album
    /// @return The amount of images in the album
    int Count() {
        return sAlbumRefreshCount;
    }

    /// @brief Return the file ID for a specified index
    /// @param index The album index
    /// @param out CapsAlbumFileId to populate
    /// @return The file ID
    int FileId(int index, CapsAlbumFileId *out) {
        if (index < 0 || index >= sAlbumRefreshCount || !out)
            return -1;
        *out = s_list[index].file_id;
        return 0;
    }

    /// @brief Return the size of the thumbnail
    /// @param index Album index
    /// @return The size
    int ThumbSize(int index) {
        if (!sInitialized || index < 0 || index >= sAlbumRefreshCount)
            return -1;
        u64 sz = 0;
        if (R_FAILED(capsaGetAlbumFileSize(&s_list[index].file_id, &sz)) || sz == 0) {
            return -1;
        }
        if (sz > 0x40000)
            return 0x40000;
        return (int)sz;
    }

    int Thumb(int index, void *out, unsigned cap) {
        if (!sInitialized || index < 0 || index >= sAlbumRefreshCount || !out || !cap)
            return -1;
        u64 done = 0;
        Result rc = capsaLoadAlbumFileThumbnail(&s_list[index].file_id, &done, out, cap);
        if (R_FAILED(rc) || done == 0 || done > cap) {
            return -1;
        }
        return (int)done;
    }

    int ImageSize(int index) {
        if (!sInitialized || index < 0 || index >= sAlbumRefreshCount)
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

    int Image(int index, void *out, unsigned cap) {
        if (!sInitialized || index < 0 || index >= sAlbumRefreshCount || !out || !cap)
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
        if (index < 0 || index >= sAlbumRefreshCount || !out || !cap)
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
        static int sMovieCount = 0;
        static int sCurrent = -1;

        static FsFileSystem sNandFilesystem;
        static bool sHasOpenedNandFs = false;

        static bool FidEqual(const CapsAlbumFileId &a, const CapsAlbumFileId &b) {
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

        /// @brief Joins two paths into one bigger path
        /// @param out A buffer to output the new path to
        /// @param outcap The maximum size to output
        /// @param a Part 1 of the new path
        /// @param b part 2 of the path
        /// @return true on sucess and false on failure
        static bool JoinPath(char *out, unsigned outcap, const char *a, const char *b) {
            /* Get string lengths */
            size_t strALen = strlen(a);
            size_t strBLen = strlen(b);
            size_t neededMemory = strALen + 1 + strBLen + 1;

            /* Prevent memory issue*/
            if (neededMemory < strALen || neededMemory > outcap) {
                return false;
            }

            /* Copy the first string to the output */
            memcpy(out, a, strALen);

            /* Actually join the paths*/
            out[strALen] = '/';
            memcpy(out + strALen + 1, b, strBLen);

            /* Apply null terminatior */
            out[strALen + 1 + strBLen] = 0;

            return true;
        }

        /// @brief Find a album photo from the SDMMC FS
        /// @param fid The album file ID to find
        /// @param want The wanted output size
        /// @param out The output path
        /// @param outcap Cap the output path
        /// @return True if succeeded, false if failed
        static bool FsFind(const CapsAlbumFileId &fid, u64 want, char *out, unsigned outcap) {
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

        /// @brief Ensure the NAND filesystem is actually there
        /// @param None
        /// @return True or false
        static bool NandEnsure(void) {
            if (sHasOpenedNandFs) {
                return true;
            }

            memset(&sNandFilesystem, 0, sizeof(sNandFilesystem));
            Result rc = fsOpenImageDirectoryFileSystem(&sNandFilesystem, FsImageDirectoryId_Nand);

            if (R_FAILED(rc)) {
                    logging::LogLine("[album] Can't open NAND filesystem (rc: 0x%X)", (unsigned)rc);
                    return false;
                }

            sHasOpenedNandFs = true;

            return true;
        }

        /// @brief Find a wanted file from the NAND
        /// @param dateTime A file date time struct
        /// @param wantedSize The desired output size
        /// @param outpath Buffer to store the output path
        /// @param outcap A cap for buffer writes
        /// @return True or false
        static bool NandFind(const CapsAlbumFileDateTime &dateTime, u64 wantedSize, char *outpath, unsigned outcap) {
            /* First part of the filename */
            char NameFirstPart[32];
            snprintf(NameFirstPart, sizeof(NameFirstPart), "%04u%02u%02u%02u%02u%02u%02u-", 
            dateTime.year, dateTime.month, dateTime.day, dateTime.hour, dateTime.minute, dateTime.second, dateTime.id);
            size_t NameFirstPartLen = strlen(NameFirstPart);

            char RootPath[128];

            for (int r = 0; r < 2; r++) {
            if (r == 0) {
                snprintf(RootPath, sizeof(RootPath), "/Album/%04u/%02u/%02u", dateTime.year, dateTime.month, dateTime.day);
            }
            else {
                snprintf(RootPath, sizeof(RootPath), "/%04u/%02u/%02u", dateTime.year, dateTime.month, dateTime.day);
            }

            FsDir dir;
            memset(&dir, 0, sizeof(dir));

            /* Try to open the directory */
            if (R_FAILED(fsFsOpenDirectory(&sNandFilesystem, RootPath, (u32)(FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles), &dir))) {
                continue;
            }

            /* A buffer for entries */
            FsDirectoryEntry enteries[64];

            bool found = false;
            while (!found) {
                s64 totalEntries = 0;

                /* Can't read the directory, so abort */
                if (R_FAILED(fsDirRead(&dir, &totalEntries, 64, enteries)) || totalEntries <= 0) {
                    break;
                }

                for (s64 i = 0; i < totalEntries; i++) {
                    /* Ignore things that aren't files */
                    if (enteries[i].type != FsDirEntryType_File) {
                        continue;
                    }

                    /* Skip anything of wrong date/time */
                    if (strncmp(enteries[i].name, NameFirstPart, NameFirstPartLen) != 0) {
                        continue;
                    }

                    /* Skip anything not of the right size */
                    if ((u64)enteries[i].file_size != wantedSize) {
                        continue;
                    }

                    /* Create the new path */
                    if (!JoinPath(outpath, outcap, RootPath, enteries[i].name)) {
                        continue;
                    }

                    found = true;
                    break;
                }
            }

            /* Close the directory*/
            fsDirClose(&dir);

            if (found) {
                return true;
            }
            }

            return false;
        }
    }
    /// @brief Find if a album index is a movie
    /// @param index The album index
    /// @return True or false
    bool IsMovie(int index) {
        if (index < 0 || index >= sAlbumRefreshCount) {
            return false;
        }

        u8 c = s_list[index].file_id.content;

        if (c == CapsAlbumFileContents_Movie || c == CapsAlbumFileContents_ExtraMovie) {
            return true;
        }

        return false;
    }
    
    /// @brief Open a movie index
    /// @param index A album index
    /// @return 
    bool MovieOpen(int index) {
        if (!sInitialized || index < 0 || index >= sAlbumRefreshCount) {
            logging::LogLine("[album] Failed to open movie (idx: %d count: %d)", index, sAlbumRefreshCount);
            return false;
        }

        const CapsAlbumFileId &AlbumFileId = s_list[index].file_id;

        int reuse = -1;

        for (int i = 0; i < sMovieCount; i++) {
            if (!s_movies[i].used || !FidEqual(s_movies[i].fid, AlbumFileId)) {
                continue;
            }

            if (s_movies[i].open) {
                sCurrent = i;
                return true;
            }

            reuse = i;
        }

        u64 AlbumFileSize = 0;
        /* Get the movie's size */
        Result szrc = capsaGetAlbumFileSize(&AlbumFileId, &AlbumFileSize);
        if (R_FAILED(szrc) || AlbumFileSize == 0) {
            logging::LogLine("[album] Failed to size movie (idx: %d rc: 0x%X)", index, (unsigned)szrc);
            return false;
        }

        int slot = reuse;

        /* Grow the movie cache/cap if nessesary*/
        if (slot < 0) {
            if (sMovieCount >= s_movieCap) {
                int ncap = s_movieCap == 0 ? 4 : s_movieCap * 2;
                MovieEntry *nn = (MovieEntry *)realloc(s_movies, (size_t)ncap * sizeof(MovieEntry));

                if (!nn) {
                    logging::LogLine("[album] Failed to grow movie cache");
                    return false;
                }

                memset(nn + s_movieCap, 0, (size_t)(ncap - s_movieCap) * sizeof(MovieEntry));
                s_movies = nn;
                s_movieCap = ncap;
            }

            slot = sMovieCount;
            sMovieCount++;
        }

        /* Create a MovieEntry for population */
        MovieEntry &e = s_movies[slot];
        std::memset(&e, 0, sizeof(e));
        e.fid = AlbumFileId;
        e.size = AlbumFileSize;

        /* Open manually from the NAND */
        if (AlbumFileId.storage == CapsAlbumStorage_Nand) {
            if (!NandEnsure()) {
                return false;
            }

            char FoundNandPath[320];
            FoundNandPath[0] = '\0';

            if (!NandFind(AlbumFileId.datetime, AlbumFileSize, FoundNandPath, sizeof(FoundNandPath))) {
                logging::LogLine("[album] Failed to find movie (ID: %d Storage: %u)", index, (unsigned)AlbumFileId.storage);
                return false;
            }

            Result frc = fsFsOpenFile(&sNandFilesystem, FoundNandPath, FsOpenMode_Read, &e.nand);

            if (R_FAILED(frc)) {
                for (int i = 0; i < sMovieCount; i++) {
                    if (s_movies[i].used && s_movies[i].open && s_movies[i].isNand) {
                        fsFileClose(&s_movies[i].nand);
                        s_movies[i].open = false;
                    }
                }

                for (int rt = 0; rt < 3 && R_FAILED(frc); rt++) {
                    svcSleepThread(200000000ull); // Retry if it fails, as session may still be closed
                    frc = fsFsOpenFile(&sNandFilesystem, FoundNandPath, FsOpenMode_Read, &e.nand);
                }

                // Now we actually can't open it
                if (R_FAILED(frc)) {
                    logging::LogLine("[album] Failed to open NAND movie (ID: %d RC: 0x%X)", index, (unsigned)frc);
                    return false;
                }
            }

            e.isNand = true;
        } else { /* Otherwise open from the SD */
            char FoundSdPath[320]; // TODO: determine if this size is good for all scenarios (should be?)
            FoundSdPath[0] = '\0';

            if (!FsFind(AlbumFileId, AlbumFileSize, FoundSdPath, sizeof(FoundSdPath))) {
                logging::LogLine("[album] Failed to find movie (ID: %d Storage: %u)", index, (unsigned)AlbumFileId.storage);
                return false;
            }

            FILE *sdFptr = fopen(FoundSdPath, "rb");

            if (!sdFptr) {
                logging::LogLine("[album] Failed to open movie (ID: %d)", index);
                return false;
            }

            e.sd = sdFptr;
            e.isNand = false;
        }

        e.used = true;
        e.open = true;
        sCurrent = slot;

        return true;
    }

    /// @brief Get the size of a movie
    /// @param entry Which movie entry
    /// @return The size of the movie. 0 is failure
    u64 MovieSize()
    {
        if (sCurrent < 0 || sCurrent >= sMovieCount) {
            return 0;
        }

        MovieEntry &e = s_movies[sCurrent];

        if (!e.used || !e.open) {
            return 0;
        }

        return e.size;
    }


    int MovieRead(u64 off, void *out, unsigned cap) {
        if (sCurrent < 0 || sCurrent >= sMovieCount || !out || !cap) {
            return -1;
        }

        MovieEntry &entry = s_movies[sCurrent];

        if (!entry.used || !entry.open || off >= entry.size) {
            return -1;
        }

        if (entry.isNand) {
            u64 BytesRead = 0;

            if (R_FAILED(fsFileRead(&entry.nand, (s64)off, out, cap, 0, &BytesRead)) || BytesRead == 0) {
                return -1;
            }

            return (int)BytesRead;
        }

        if (fseeko(entry.sd, (off_t)off, SEEK_SET) != 0) {
            return -1;
        }

        size_t n = fread(out, 1, cap, entry.sd);
        if (n == 0) {
            return -1;
        }

        return (int)n;
    }

    void MovieClose()
    {
        sCurrent = -1;
    }

} // namespace album
