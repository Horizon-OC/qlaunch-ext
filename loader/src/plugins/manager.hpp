/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering...         */

/* Plugin framework entry point.
 *
 * This directory is the plugin interface: one binding per part of the
 * system that plugins can inject themselves into.
 *
 *   manager.*  lifecycle (load/unload/loop) + this overview.
 *   vtitles.*  TitlePlugin binding: virtual titles in the menu.
 *
 * Plugin authors: mirror SDK headers live in plugins/include/qlaunch/.
 */

#pragma once
#include <switch.h>

namespace plugins {

/// @brief Discover, load and boot all enabled plugins.
/// @param win Native window handle passed to each onBoot hook.
void Boot(NWindow *win);

/// @brief Pump every loaded plugin for one tick.
/// @param inGame True while a game holds the foreground (non-persistent
/// plugins are skipped but stay loaded).
void Loop(bool inGame);

/// @brief Notify all plugins that the menu opened.
/// @details Calls each plugin's onOpen hook, if exported.
void OpenAll();

/// @brief Notify all plugins of a power-button press.
/// @details Calls each plugin's onPowerButton hook, if exported.
void PowerAll();

/// @brief Shut down and unload all plugins in reverse order.
void Shutdown();

} /* namespace plugins */
