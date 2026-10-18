#pragma once
#include "MenuHost.hpp"

#ifndef QLAUNCH_HOOK
#define QLAUNCH_HOOK __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    enum {
        kWidgetApiVersion = 1
    };

    /// @brief Give the widget ABI version.
    /// @return kWidgetApiVersion.
    QLAUNCH_HOOK int WidgetApiVersion(void);

    /// @brief Give the widget name.
    /// @return Name owned by the dnro.
    QLAUNCH_HOOK const char *WidgetName(void);

    /// @brief Boot the widget with the menu host API.
    /// @param hostApi All menu functions plus the qext family.
    /// @return 0 to stay loaded.
    QLAUNCH_HOOK int WidgetBoot(const struct MenuHostApi *hostApi);

    /// @brief Draw the widget onto the core's state.
    /// @param hostApi Host API.
    /// @param drawContext Draw rectangle in design space.
    QLAUNCH_HOOK void WidgetDraw(const struct MenuHostApi *hostApi,
                                 const struct WidgetDrawContext *drawContext);

    /// @brief Tick the widget once per frame.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void WidgetTick(const struct MenuHostApi *hostApi);

    /// @brief Shut down before unload.
    QLAUNCH_HOOK void WidgetShutdown(void);

#ifdef __cplusplus
}
#endif
