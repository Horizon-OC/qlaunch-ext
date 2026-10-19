/* NekoMenu Album submenu (C) 2026 Souldbminer */
/* Licensed under the GPLv2             */

#pragma once
#include <switch.h>
#include "MenuHost.hpp"
#include "Input.hpp"
#include <cstdio>
#include <cstdarg>
#include <cstdint>

/// @brief Host table installed by SubmenuBoot; valid until SubmenuShutdown.
/// Threaded through a single pointer (same pattern as the core s_hostApi)
/// so the transplanted widget logic stays close to its menu origins.
inline const MenuHostApi *g_host = nullptr;

/// @brief Log a formatted line through the host.
/// @param format printf-style format.
inline void LogFmt(const char *format, ...)
{
    if (!g_host || !format) {
        return;
    }

    char textBuffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(textBuffer, sizeof(textBuffer), format, args);
    va_end(args);
    textBuffer[sizeof(textBuffer) - 1] = 0;
    g_host->logText(textBuffer);
}

namespace Album {

    /// @brief Whether the album viewer is open.
    /// @return True when open.
    bool IsOpen();

    /// @brief Whether the album viewer covers the screen.
    /// @return True when grid-fullscreen or video is open.
    bool IsFull();

    /// @brief Open the album grid and refresh thumbnails.
    void Open();

    /// @brief Open one image fullscreen (images only, not movies).
    /// @param albumEntryIndex Album entry index.
    void OpenFull(int albumEntryIndex);

    /// @brief Drop the fullscreen image texture.
    void DropFull();

    /// @brief Drop all thumbnail textures.
    void DropThumbs();

    /// @brief Close the album viewer and free its buffers.
    void Close();

    /// @brief Drop cached textures after an external refresh.
    void OnRefresh();

    /// @brief Draw the active viewer phase.
    void Draw();

    /// @brief Handle album grid/full navigation input.
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    /// @return True when the input was consumed.
    bool Input(unsigned long long buttonsDown, unsigned long long buttonsHeld);

} /* namespace Album */

namespace Video {

    /// @brief Whether a video is currently open.
    /// @return True when open.
    bool IsOpen();

    /// @brief Open a movie entry for playback.
    /// @param albumEntryIndex Album entry index.
    /// @return True on success.
    bool Open(int albumEntryIndex);

    /// @brief Close the video player.
    void Close();

    /// @brief Drop pinned video textures.
    void DropPinned();

    /// @brief Put new frames on GPU. Call each tick.
    void Tick();

    /// @brief Draw the current video frame and OSD.
    void Draw();

    /// @brief Handle video playback input.
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    /// @return True when the input was consumed.
    bool Input(unsigned long long buttonsDown, unsigned long long buttonsHeld);

    /// @brief Probe (and cache) a movie's duration.
    /// @param albumEntryIndex Album entry index.
    /// @return Duration in seconds, or negative when unknown.
    double Duration(int albumEntryIndex);

    /// @brief Clear cached durations.
    void DurReset();

} /* namespace Video */
