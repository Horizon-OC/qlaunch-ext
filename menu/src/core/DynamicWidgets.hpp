#pragma once
#include "MenuHost.hpp"

namespace DynamicWidgets {

    /// @brief Boot dynamic widgets from the NekoMenu directory.
    /// @param hostApi Host table with menu functions plus qext.
    void Boot(const MenuHostApi *hostApi);

    /// @brief Shut down widgets and unload dnro files.
    void Shutdown();

    /// @brief Tick loaded widgets once per frame.
    void Tick();

    /// @brief Whether a named widget is loaded.
    /// @param widgetName Widget name.
    /// @return True when loaded.
    bool HasWidget(const char *widgetName);

    /// @brief Draw all widgets onto the core state.
    void DrawAll();

} /* namespace DynamicWidgets */
