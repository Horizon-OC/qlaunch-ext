#pragma once
#include "MenuHost.hpp"

#ifndef QLAUNCH_HOOK
#define QLAUNCH_HOOK __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    enum {
        kSubmenuApiVersion = 1
    };

    /// @brief Give the submenu ABI version.
    /// @return kSubmenuApiVersion.
    QLAUNCH_HOOK int SubmenuApiVersion(void);

    /// @brief Give the stable submenu id.
    /// @return Lowercase id owned by the dnro.
    QLAUNCH_HOOK const char *SubmenuId(void);

    /// @brief Give the display title.
    /// @return Title owned by the dnro.
    QLAUNCH_HOOK const char *SubmenuTitle(void);

    /// @brief Give the jingle clip name, if any.
    /// @return Clip name or NULL.
    QLAUNCH_HOOK const char *SubmenuJingle(void);

    /// @brief Give the fallback glyph, if any.
    /// @return Codepoint or 0.
    QLAUNCH_HOOK unsigned SubmenuGlyph(void);

    /// @brief Give the embedded icon PNG bytes, if any.
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL.
    QLAUNCH_HOOK const unsigned char *SubmenuIconPng(unsigned *outputSize);

    /// @brief Give the embedded selected-state icon PNG bytes, if any.
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL to reuse the normal icon.
    QLAUNCH_HOOK const unsigned char *SubmenuSelectedIconPng(unsigned *outputSize);

    /// @brief Give the submenu accent color.
    /// @param hostApi Host API for theme-derived colors.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    /// @return Nonzero when a color is provided.
    QLAUNCH_HOOK int SubmenuColor(const struct MenuHostApi *hostApi,
                                  float *redOut, float *greenOut, float *blueOut);

    /// @brief Give the submenu background color.
    /// @param hostApi Host API for theme-derived colors.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    /// @return Nonzero when a color is provided.
    QLAUNCH_HOOK int SubmenuBackgroundColor(const struct MenuHostApi *hostApi,
                                            float *redOut, float *greenOut, float *blueOut);

    /// @brief Boot the submenu with the menu host API.
    /// @param hostApi All menu functions plus the qext family.
    /// @return 0 to stay loaded.
    QLAUNCH_HOOK int SubmenuBoot(const struct MenuHostApi *hostApi);

    /// @brief Draw the submenu onto the core's state.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuDraw(const struct MenuHostApi *hostApi);

    /// @brief Handle submenu input.
    /// @param hostApi Host API.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    QLAUNCH_HOOK void SubmenuInput(const struct MenuHostApi *hostApi,
                                   unsigned long long buttonsDown,
                                   unsigned long long buttonsHeld);

    /// @brief Enter the submenu tab.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuOpen(const struct MenuHostApi *hostApi);

    /// @brief Exit the submenu tab.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuClose(const struct MenuHostApi *hostApi);

    /// @brief Tick the submenu once per frame.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuTick(const struct MenuHostApi *hostApi);

    /// @brief Whether the submenu currently covers the screen.
    /// @return Nonzero when fullscreen photo or video is showing.
    QLAUNCH_HOOK int SubmenuIsFull(void);

    /// @brief Whether the submenu wants all input routed to it.
    /// @return Nonzero when open (grid or fullscreen).
    QLAUNCH_HOOK int SubmenuInputLocked(void);

    /// @brief Drop cached textures after an external refresh.
    QLAUNCH_HOOK void SubmenuRefresh(void);


    /// @brief Shut down before unload.
    QLAUNCH_HOOK void SubmenuShutdown(void);

#ifdef __cplusplus
}
#endif
