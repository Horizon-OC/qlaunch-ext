/* qlaunch-ext theme pack resolver (C) 2026 Souldbminer */
/* Licensed under the GPLv2                          */
#pragma once

namespace Packs {

    /// @brief Maximum theme pack name length (no path separators).
    constexpr int kPackNameMax = 63;

    /// @brief Resolve the active theme pack directory from theme.ini.
    /// Falls back to the default pack on any missing or hostile value.
    /// @param packDirOut Output buffer for "sdmc:/qlaunch-ext/NekoMenu/<pack>".
    /// @param packDirCapacity Output capacity.
    /// @return True when a directory was written.
    bool Resolve(char *packDirOut, unsigned packDirCapacity);

    /// @brief Read a whole SD file into a malloc'd buffer.
    /// @param path Filesystem path.
    /// @param sizeCap Maximum accepted bytes.
    /// @param sizeOut Output byte size (untouched on failure).
    /// @return Buffer to free(), or null on any failure.
    unsigned char *ReadFile(const char *path, unsigned sizeCap, unsigned *sizeOut);

    /// @brief Join a pack asset name onto "<packDir>/assets/".
    /// "grass.png" becomes "<packDir>/assets/grass.png"; subdirectories
    /// like "hud/icon.png" or "sfx/click.wav" are allowed. Absolute
    /// paths, traversal (".."), and hostile characters are rejected.
    /// @param packDir Resolved pack directory (never null).
    /// @param assetName Pack-relative asset name.
    /// @param pathOut Output path buffer.
    /// @param pathCapacity Output capacity.
    /// @return True when a path was written.
    bool AssetPath(const char *packDir, const char *assetName, char *pathOut, unsigned pathCapacity);

    /// @brief Read a pack asset into a malloc'd buffer.
    /// @param packDir Resolved pack directory (never null).
    /// @param assetName Pack-relative asset name (for example "grass.png").
    /// @param sizeCap Maximum accepted bytes.
    /// @param sizeOut Output byte size (untouched on failure).
    /// @return Buffer to free(), or null on any failure.
    unsigned char *ReadAsset(const char *packDir, const char *assetName, unsigned sizeCap,
                             unsigned *sizeOut);

    /// @brief Give the active pack directory, resolved once and cached.
    /// @return Pack directory (for example "sdmc:/qlaunch-ext/NekoMenu/default").
    const char *ActiveDir();

} /* namespace Packs */
