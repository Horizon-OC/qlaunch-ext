#pragma once
#include "MenuHost.hpp"

#ifndef QLAUNCH_HOOK
#define QLAUNCH_HOOK __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    constexpr int kSubmenuApiVersion = 1;

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

    /// @brief Act on a registered touch cell (select on tap, act on re-tap).
    /// Optional; missing means taps fall through to an A press.
    /// @param cellIndex Cell index from touchRegisterCell.
    /// @return Nonzero when the tap was consumed.
    QLAUNCH_HOOK int SubmenuTouchAction(int cellIndex);

    /// @brief Drag-scroll tab content with inertia on release.
    /// Optional; missing means the tab ignores drags.
    /// @param moveX Horizontal finger travel this frame.
    /// @param moveY Vertical finger travel this frame.
    /// @param phase Touch phase (held moves, up releases).
    /// @return Nonzero when the drag was consumed.
    QLAUNCH_HOOK int SubmenuDrag(float moveX, float moveY, int phase);

    /// @brief Handle one tap inside the tab (manual override).
    /// Optional; checked before registered cells. Home uses this for
    /// toggle/deselect semantics that a plain select/act cannot express.
    /// Most submenus should register cells and implement SubmenuTouchAction
    /// instead, returning zero here for unhandled areas.
    /// @param x Tap X in design space.
    /// @param y Tap Y in design space.
    /// @return Nonzero when the tap was consumed.
    QLAUNCH_HOOK int SubmenuTap(float x, float y);

    /// @brief Handle a finger touching down inside the tab (press callback).
    /// Optional; returning nonzero claims the finger, skipping drag,
    /// swipe, and tap routing for it and delivering SubmenuTouchUp on
    /// release. Returning zero keeps the standard gesture path.
    /// @param x Down X in design space.
    /// @param y Down Y in design space.
    /// @return Nonzero when the finger was claimed.
    QLAUNCH_HOOK int SubmenuTouchDown(float x, float y);

    /// @brief Handle a claimed finger lifting off (release callback).
    /// Only fires for fingers claimed by SubmenuTouchDown.
    /// @param x Up X in design space.
    /// @param y Up Y in design space.
    /// @return Nonzero when the release was consumed.
    QLAUNCH_HOOK int SubmenuTouchUp(float x, float y);

    /// @brief Shut down before unload.
    QLAUNCH_HOOK void SubmenuShutdown(void);

#ifdef __cplusplus
}
#endif
