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

    /// @brief Maximum pack asset name length (allows "hud/icon.png" style subpaths).
    constexpr int kAssetNameMax = 127;

    namespace {

        /// @brief Cached active pack directory (resolved once at first use).
        static char s_activeDir[128] = { 0 };

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

    /// @brief Give the active pack directory, resolved once and cached.
    /// @return Pack directory (for example "sdmc:/qlaunch-ext/NekoMenu/default").
    const char *ActiveDir()
    {
        if (!s_activeDir[0] && !Resolve(s_activeDir, sizeof(s_activeDir))) {
            snprintf(s_activeDir, sizeof(s_activeDir), "%s/%s", kNekoMenuDir, kDefaultPack);
            s_activeDir[sizeof(s_activeDir) - 1] = 0;
        }

        return s_activeDir;
    }

    /// @brief Join a pack asset name onto "<packDir>/assets/".
    /// "grass.png" becomes "<packDir>/assets/grass.png"; subdirectories
    /// like "hud/icon.png" or "sfx/click.wav" are allowed. Absolute
    /// paths, traversal (".."), and hostile characters are rejected.
    /// @param packDir Resolved pack directory (never null).
    /// @param assetName Pack-relative asset name.
    /// @param pathOut Output path buffer.
    /// @param pathCapacity Output capacity.
    /// @return True when a path was written.
    bool AssetPath(const char *packDir, const char *assetName, char *pathOut, unsigned pathCapacity)
    {
        static const char kAssetsDir[] = "/assets/";

        if (!packDir || !packDir[0] || !assetName || !assetName[0] || !pathOut || pathCapacity == 0) {
            return false;
        }

        if (assetName[0] == '/' || strlen(assetName) > (size_t)kAssetNameMax) {
            return false;
        }

        if (strstr(assetName, "..") != nullptr) {
            return false;
        }

        for (const char *cursor = assetName; *cursor; cursor++) {
            char current = *cursor;

            if (current == '\\' || current == '"' || current == ':') {
                return false;
            }

            if ((unsigned char)current < 32) {
                return false;
            }
        }

        size_t dirLength = strlen(packDir);
        size_t assetsLength = strlen(kAssetsDir);
        size_t nameLength = strlen(assetName);

        if (dirLength + assetsLength + nameLength + 1 > pathCapacity) {
            return false;
        }

        memcpy(pathOut, packDir, dirLength);
        memcpy(pathOut + dirLength, kAssetsDir, assetsLength);
        memcpy(pathOut + dirLength + assetsLength, assetName, nameLength + 1);
        return true;
    }

    /// @brief Read a pack asset into a malloc'd buffer.
    /// @param packDir Resolved pack directory (never null).
    /// @param assetName Pack-relative asset name (for example "grass.png").
    /// @param sizeCap Maximum accepted bytes.
    /// @param sizeOut Output byte size (untouched on failure).
    /// @return Buffer to free(), or null on any failure.
    unsigned char *ReadAsset(const char *packDir, const char *assetName, unsigned sizeCap,
                             unsigned *sizeOut)
    {
        char assetPath[256] = { 0 };

        if (!AssetPath(packDir, assetName, assetPath, sizeof(assetPath))) {
            return nullptr;
        }

        return ReadFile(assetPath, sizeCap, sizeOut);
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
