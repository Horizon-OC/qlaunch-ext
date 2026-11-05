#pragma once
#include "MenuHost.hpp"

#ifndef QLAUNCH_HOOK
#define QLAUNCH_HOOK __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /// Menu buttons live in theme packs (<pack>/plugins/*.dnro) and are
    /// drawn by the layout in the bottom bar, right side, for one tab.
    /// A plugin registers any number of buttons; the layout filters them
    /// by tab id, shows the label under the button, and routes A presses
    /// (and taps, where the layout supports them) to MenuButtonActivate.

    constexpr int kMenuPluginApiVersion = 1;

    /// @brief Give the menu-plugin ABI version.
    /// @return kMenuPluginApiVersion.
    QLAUNCH_HOOK int MenuPluginApiVersion(void);

    /// @brief Give the stable plugin id.
    /// @return Lowercase id owned by the dnro.
    QLAUNCH_HOOK const char *MenuPluginId(void);

    /// @brief Boot the plugin with the menu host API.
    /// @param hostApi All menu functions plus the qext family.
    /// @return 0 to stay loaded.
    QLAUNCH_HOOK int MenuPluginBoot(const struct MenuHostApi *hostApi);

    /// @brief Give the registered button count.
    /// @return Button count (0 for none).
    QLAUNCH_HOOK int MenuButtonCount(void);

    /// @brief Give the tab a button belongs to.
    /// @param buttonIndex Button index (0..MenuButtonCount-1).
    /// @return Tab id (for example "home"), or NULL to hide the button.
    QLAUNCH_HOOK const char *MenuButtonTab(int buttonIndex);

    /// @brief Give a button's label, drawn under the button.
    /// @param buttonIndex Button index (0..MenuButtonCount-1).
    /// @return Label owned by the dnro, or NULL for no label.
    QLAUNCH_HOOK const char *MenuButtonLabel(int buttonIndex);

    /// @brief Give a button's icon PNG bytes, if any.
    /// Buttons without icons draw the label text instead.
    /// @param buttonIndex Button index (0..MenuButtonCount-1).
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL.
    QLAUNCH_HOOK const unsigned char *MenuButtonIconPng(int buttonIndex, unsigned *outputSize);

    /// @brief Activate a button (A press or tap while focused).
    /// @param hostApi Host API.
    /// @param buttonIndex Button index (0..MenuButtonCount-1).
    /// @return Nonzero when the press was consumed.
    QLAUNCH_HOOK int MenuButtonActivate(const struct MenuHostApi *hostApi, int buttonIndex);

    /// @brief Shut down before unload.
    QLAUNCH_HOOK void MenuPluginShutdown(void);

#ifdef __cplusplus
}
#endif
