/* qlaunch-ext theme pack resolver (C) 2026 Souldbminer */
/* Licensed under the GPLv2                          */

#include "Packs.hpp"
#include <minIni.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace Packs {

    namespace {

        constexpr char kNekoMenuDir[] = "sdmc:/qlaunch-ext/NekoMenu";
        constexpr char kThemeIni[] = "sdmc:/qlaunch-ext/NekoMenu/theme.ini";
        constexpr char kDefaultPack[] = "default";

        /// @brief Whether a pack name is safe to join into an SD path.
        /// @param packName Candidate name from theme.ini.
        /// @return True when non-empty and separator/traversal free.
        bool IsPackNameUsable(const char *packName)
        {
            if (!packName || !packName[0]) {
                return false;
            }

            if (strlen(packName) > (size_t)kPackNameMax) {
                return false;
            }

            for (const char *cursor = packName; *cursor; cursor++) {
                char current = *cursor;

                if (current == '/' || current == '\\' || current == '"' || current == ':') {
                    return false;
                }

                if ((unsigned char)current < 32) {
                    return false;
                }
            }

            if (strstr(packName, "..") != nullptr) {
                return false;
            }

            return true;
        }

    } // namespace

    /// @brief Resolve the active theme pack directory from theme.ini.
    /// Falls back to the default pack on any missing or hostile value.
    /// @param packDirOut Output buffer for "sdmc:/qlaunch-ext/NekoMenu/<pack>".
    /// @param packDirCapacity Output capacity.
    /// @return True when a directory was written.
    bool Resolve(char *packDirOut, unsigned packDirCapacity)
    {
        if (!packDirOut || packDirCapacity == 0) {
            return false;
        }

        char packName[kPackNameMax + 1] = { 0 };
        ini_gets("theme", "themeName", "", packName, (int)sizeof(packName), kThemeIni);

        if (!packName[0]) {
            ini_gets("theme", "name", "", packName, (int)sizeof(packName), kThemeIni);
        }

        if (!IsPackNameUsable(packName)) {
            snprintf(packName, sizeof(packName), "%s", kDefaultPack);
            packName[sizeof(packName) - 1] = 0;
        }

        size_t baseLength = strlen(kNekoMenuDir);
        size_t nameLength = strlen(packName);

        if (baseLength + 1 + nameLength + 1 > packDirCapacity) {
            return false;
        }

        memcpy(packDirOut, kNekoMenuDir, baseLength);
        packDirOut[baseLength] = '/';
        memcpy(packDirOut + baseLength + 1, packName, nameLength + 1);
        return true;
    }

    /// @brief Read a whole SD file into a malloc'd buffer.
    /// @param path Filesystem path.
    /// @param sizeCap Maximum accepted bytes.
    /// @param sizeOut Output byte size (untouched on failure).
    /// @return Buffer to free(), or null on any failure.
    unsigned char *ReadFile(const char *path, unsigned sizeCap, unsigned *sizeOut)
    {
        if (!path || !path[0] || sizeCap == 0) {
            return nullptr;
        }

        FILE *sourceFile = fopen(path, "rb");

        if (!sourceFile) {
            return nullptr;
        }

        fseek(sourceFile, 0, SEEK_END);
        long fileSize = ftell(sourceFile);
        fseek(sourceFile, 0, SEEK_SET);

        if (fileSize <= 0 || (unsigned)fileSize > sizeCap) {
            fclose(sourceFile);
            return nullptr;
        }

        unsigned char *fileData = (unsigned char *)malloc((unsigned)fileSize);

        if (!fileData) {
            fclose(sourceFile);
            return nullptr;
        }

        size_t fileGot = fread(fileData, 1, (unsigned)fileSize, sourceFile);
        fclose(sourceFile);

        if (fileGot != (unsigned)fileSize) {
            free(fileData);
            return nullptr;
        }

        if (sizeOut) {
            *sizeOut = (unsigned)fileSize;
        }

        return fileData;
    }

} /* namespace Packs */
