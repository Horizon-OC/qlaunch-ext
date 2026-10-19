/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

constexpr int kTabMax = 12;
constexpr int kTabIdMax = 24;
constexpr int kTabTitleMax = 48;
constexpr int kTabJingleMax = 32;

/// @brief Draw callback for a menu tab.
typedef void (*TabDrawFn)(void);

/// @brief Input callback for a menu tab.
typedef void (*TabInputFn)(u64 buttonsDown, u64 buttonsHeld);

/// @brief Lifecycle callback for a menu tab.
typedef void (*TabLifeFn)(void);

/// @brief Descriptor for one plugin-driven menu tab.
///
/// The jingle is a data-driven SFX clip name (see sfx.json), not an enum:
/// layout.json may override it per tab, and App plays it via
/// Sfx::PlayByName() when the tab becomes active.
struct MenuTab {
    char id[kTabIdMax];
    char title[kTabTitleMax];
    unsigned glyph;
    int hudSel;
    int hudDesel;
    char jingle[kTabJingleMax];
    TabDrawFn draw;
    TabInputFn input;
    TabLifeFn onEnter;
    TabLifeFn onExit;
};

namespace Tabs {

    /// @brief Register a tab. Returns its index, or -1 when full.
    /// @param tab Descriptor to copy into the registry.
    /// @return Index of the tab, or -1 on failure.
    int Register(const MenuTab &tab);

    /// @brief Number of registered tabs.
    /// @return Tab count.
    int Count();

    /// @brief Look up a tab by index.
    /// @param i Tab index.
    /// @return Pointer to the tab, or null when out of range.
    const MenuTab *Get(int i);

    /// @brief Look up a tab by string id.
    /// @param id Tab id (for example "home").
    /// @return Index of the tab, or -1 when missing.
    int FindById(const char *id);

    /// @brief Override the jingle clip for a registered tab.
    /// @param i Tab index from Register() or FindById().
    /// @param jingle SFX clip name (for example "home_jingle"), or null/empty to mute.
    /// @return True on success.
    bool SetJingle(int i, const char *jingle);

    /// @brief Clear the registry. Used by tests and reboot.
    void Clear();

    /// @brief Remove the tab at an index, shifting later tabs down.
    /// @param tabIndex Tab index to remove.
    /// @return True on success.
    bool RemoveAt(int tabIndex);

    /// @brief Override the icon slots for a registered tab.
    /// @param tabIndex Tab index from Register() or FindById().
    /// @param selectedSlot HUD icon slot when selected.
    /// @param deselectedSlot HUD icon slot when deselected.
    /// @return True on success.
    bool SetIconSlots(int tabIndex, int selectedSlot, int deselectedSlot);

} /* namespace Tabs */
