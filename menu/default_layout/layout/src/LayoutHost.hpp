/* layout private host pointer (C) 2026 Souldbminer */
/* Licensed under the GPLv2                        */
#pragma once
#include "MenuHost.hpp"

/// @brief Install the host table (called once by LayoutBoot).
/// @param hostApi Host API implemented by menu.dnro.
void SetLayoutHost(const MenuHostApi *hostApi);

/// @brief Give the installed host table.
/// @return Host API (never null after boot).
const MenuHostApi *GetLayoutHost();
