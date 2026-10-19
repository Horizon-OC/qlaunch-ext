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

} /* namespace Packs */
