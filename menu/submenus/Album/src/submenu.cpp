/* NekoMenu Album submenu (C) 2026 Souldbminer */
/* Licensed under the GPLv2             */

#include "common.hpp"
#include "SubmenuApi.hpp"
#include <cstdio>
#include <stdint.h>

extern const uint8_t top_sel_album_png[];
extern const uint32_t top_sel_album_png_size;
extern const uint8_t top_des_album_png[];
extern const uint32_t top_des_album_png_size;

extern "C" {

    /// @brief Give the submenu ABI version.
    /// @return kSubmenuApiVersion.
    QLAUNCH_HOOK int SubmenuApiVersion(void)
    {
        return kSubmenuApiVersion;
    }

    /// @brief Give the stable submenu id.
    /// @return Lowercase id owned by the dnro.
    QLAUNCH_HOOK const char *SubmenuId(void)
    {
        return "album";
    }

    /// @brief Give the display title.
    /// @return Title owned by the dnro.
    QLAUNCH_HOOK const char *SubmenuTitle(void)
    {
        return "Album";
    }

    /// @brief Give the jingle clip name, if any.
    /// @return Clip name or NULL.
    QLAUNCH_HOOK const char *SubmenuJingle(void)
    {
        return "album_jingle";
    }

    /// @brief Give the fallback glyph, if any.
    /// @return Codepoint or 0.
    QLAUNCH_HOOK unsigned SubmenuGlyph(void)
    {
        return 0xE134;
    }

    /// @brief Give the embedded icon PNG bytes, if any.
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL.
    QLAUNCH_HOOK const unsigned char *SubmenuIconPng(unsigned *outputSize)
    {
        if (outputSize) {
            *outputSize = top_sel_album_png_size;
        }

        return top_sel_album_png;
    }

    /// @brief Give the embedded selected-state icon PNG bytes, if any.
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL to reuse the normal icon.
    QLAUNCH_HOOK const unsigned char *SubmenuSelectedIconPng(unsigned *outputSize)
    {
        if (outputSize) {
            *outputSize = top_des_album_png_size;
        }

        return top_des_album_png;
    }

    /// @brief Give the submenu accent color.
    /// @param hostApi Host API for theme-derived colors.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    /// @return Nonzero when a color is provided.
    QLAUNCH_HOOK int SubmenuColor(const MenuHostApi *hostApi,
                                  float *redOut, float *greenOut, float *blueOut)
    {
        if (!hostApi || !redOut || !greenOut || !blueOut) {
            return 0;
        }

        hostApi->themeTabAccent("album", redOut, greenOut, blueOut);
        return 1;
    }

    /// @brief Give the submenu background color.
    /// @param hostApi Host API for theme-derived colors.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    /// @return Nonzero when a color is provided.
    QLAUNCH_HOOK int SubmenuBackgroundColor(const MenuHostApi *hostApi,
                                            float *redOut, float *greenOut, float *blueOut)
    {
        if (!hostApi || !redOut || !greenOut || !blueOut) {
            return 0;
        }

        hostApi->themeTabColor("album", redOut, greenOut, blueOut);
        return 1;
    }

    /// @brief Boot the submenu with the menu host API.
    /// @param hostApi All menu functions plus the qext family.
    /// @return 0 to stay loaded.
    QLAUNCH_HOOK int SubmenuBoot(const MenuHostApi *hostApi)
    {
        if (!hostApi || hostApi->version != kMenuHostApiVersion) {
            return -1;
        }

        if (!hostApi->gfxPushOverlayPanel) {
            return -1;
        }

        g_host = hostApi;
        return 0;
    }

    /// @brief Draw the submenu onto the core's state.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuDraw(const MenuHostApi *hostApi)
    {
        g_host = hostApi;
        Album::Draw();
    }

    /// @brief Handle submenu input.
    /// @param hostApi Host API.
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    QLAUNCH_HOOK void SubmenuInput(const MenuHostApi *hostApi,
                                   unsigned long long buttonsDown,
                                   unsigned long long buttonsHeld)
    {
        g_host = hostApi;
        Album::Input(buttonsDown, buttonsHeld);
    }

    /// @brief Enter the submenu tab.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuOpen(const MenuHostApi *hostApi)
    {
        g_host = hostApi;
        Album::Open();
    }

    /// @brief Exit the submenu tab.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuClose(const MenuHostApi *hostApi)
    {
        g_host = hostApi;
        Album::Close();
    }

    /// @brief Tick the submenu once per frame.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuTick(const MenuHostApi *hostApi)
    {
        g_host = hostApi;
        Video::Tick();
    }

    /// @brief Whether the submenu currently covers the screen.
    /// @return Nonzero when fullscreen photo or video is showing.
    QLAUNCH_HOOK int SubmenuIsFull(void)
    {
        return (Album::IsFull() || Video::IsOpen()) ? 1 : 0;
    }

    /// @brief Whether the submenu wants all input routed to it.
    /// @return Nonzero when open (grid or fullscreen).
    QLAUNCH_HOOK int SubmenuInputLocked(void)
    {
        return (Album::IsOpen() || Video::IsOpen()) ? 1 : 0;
    }

    /// @brief Drop cached textures after an external refresh.
    QLAUNCH_HOOK void SubmenuRefresh(void)
    {
        Album::OnRefresh();
    }

    /// @brief Shut down before unload.
    QLAUNCH_HOOK void SubmenuShutdown(void)
    {
        if (Video::IsOpen()) {
            Video::Close();
        }

        Video::DropPinned();
        Album::Close();
        g_host = nullptr;
    }

} /* extern C */
