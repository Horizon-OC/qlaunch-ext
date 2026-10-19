/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Tabs.hpp"
#include <cstring>

namespace Tabs {

    namespace {

        /// @brief Tab registry, single global for all menu tabs.
        struct TabRegistry {
            MenuTab tabList[kTabMax];
            int tabCount;
        };

        static TabRegistry s_registry = {};

        /// @brief Fill a MenuTab with safe defaults.
        /// @param tabDescriptor Tab to initialize.
        void ClearTab(MenuTab &tabDescriptor)
        {
            memset(&tabDescriptor, 0, sizeof(tabDescriptor));
            tabDescriptor.glyph = 0;
            tabDescriptor.hudSel = -1;
            tabDescriptor.hudDesel = -1;
            tabDescriptor.jingle[0] = 0;
            tabDescriptor.draw = NULL;
            tabDescriptor.input = NULL;
            tabDescriptor.onEnter = NULL;
            tabDescriptor.onExit = NULL;
        }

        /// @brief Copy a C string with truncation safety.
        /// @param destinationBuffer Destination buffer.
        /// @param destinationCapacity Size of the destination buffer.
        /// @param sourceText Source string.
        static void CopyStr(char *destinationBuffer, unsigned destinationCapacity, const char *sourceText)
        {
            if (!destinationBuffer || !destinationCapacity) {
                return;
            }

            if (!sourceText) {
                destinationBuffer[0] = 0;
                return;
            }

            size_t sourceLength = strlen(sourceText);

            if (sourceLength > (size_t)(destinationCapacity - 1)) {
                sourceLength = (size_t)(destinationCapacity - 1);
            }

            memcpy(destinationBuffer, sourceText, sourceLength);
            destinationBuffer[sourceLength] = 0;
        }

        /// @brief Copy a jingle clip name with truncation safety.
        /// @param destinationTab Destination tab receiving the jingle.
        /// @param jingleName SFX clip name, or null/empty to mute.
        void CopyJingle(MenuTab &destinationTab, const char *jingleName)
        {
            if (!jingleName || !jingleName[0]) {
                destinationTab.jingle[0] = 0;
                return;
            }

            CopyStr(destinationTab.jingle, sizeof(destinationTab.jingle), jingleName);
        }

    } // namespace

    /// @brief Register a tab. Returns its index, or -1 when full.
    /// @param sourceTab Descriptor to copy into the registry.
    /// @return Index of the tab, or -1 on failure.
    int Register(const MenuTab &sourceTab)
    {
        if (s_registry.tabCount < 0 || s_registry.tabCount >= kTabMax) {
            return -1;
        }

        /* Note: draw may be null (home renders via the gamestrip node). */
        if (sourceTab.id[0] == 0) {
            return -1;
        }

        for (int tabIndex = 0; tabIndex < s_registry.tabCount; tabIndex++) {
            if (strncmp(s_registry.tabList[tabIndex].id, sourceTab.id, kTabIdMax) == 0) {
                return -1;
            }
        }

        s_registry.tabList[s_registry.tabCount] = sourceTab;
        s_registry.tabList[s_registry.tabCount].id[kTabIdMax - 1] = 0;
        s_registry.tabList[s_registry.tabCount].title[kTabTitleMax - 1] = 0;
        return s_registry.tabCount++;
    }

    /// @brief Number of registered tabs.
    /// @return Tab count.
    int Count()
    {
        return s_registry.tabCount;
    }

    /// @brief Look up a tab by index.
    /// @param tabIndex Tab index.
    /// @return Pointer to the tab, or null when out of range.
    const MenuTab *Get(int tabIndex)
    {
        if (tabIndex < 0 || tabIndex >= s_registry.tabCount) {
            return NULL;
        }

        return &s_registry.tabList[tabIndex];
    }

    /// @brief Look up a tab by string id.
    /// @param tabId Tab id (for example "home").
    /// @return Index of the tab, or -1 when missing.
    int FindById(const char *tabId)
    {
        if (!tabId || !tabId[0]) {
            return -1;
        }

        for (int tabIndex = 0; tabIndex < s_registry.tabCount; tabIndex++) {
            if (strncmp(s_registry.tabList[tabIndex].id, tabId, kTabIdMax) == 0) {
                return tabIndex;
            }
        }

        return -1;
    }

    /// @brief Override the jingle clip for a registered tab.
    /// @param tabIndex Tab index from Register() or FindById().
    /// @param jingleName SFX clip name (for example "home_jingle"), or null/empty to mute.
    /// @return True on success.
    bool SetJingle(int tabIndex, const char *jingleName)
    {
        if (tabIndex < 0 || tabIndex >= s_registry.tabCount) {
            return false;
        }

        CopyJingle(s_registry.tabList[tabIndex], jingleName);
        return true;
    }

    /// @brief Clear the registry. Used by tests and reboot.
    void Clear()
    {
        for (int tabIndex = 0; tabIndex < kTabMax; tabIndex++) {
            ClearTab(s_registry.tabList[tabIndex]);
        }

        s_registry.tabCount = 0;
    }

    /// @brief Remove the tab at an index, shifting later tabs down.
    /// @param tabIndex Tab index to remove.
    /// @return True on success.
    bool RemoveAt(int tabIndex)
    {
        if (tabIndex < 0 || tabIndex >= s_registry.tabCount) {
            return false;
        }

        for (int moveIndex = tabIndex; moveIndex + 1 < s_registry.tabCount; moveIndex++) {
            s_registry.tabList[moveIndex] = s_registry.tabList[moveIndex + 1];
        }

        ClearTab(s_registry.tabList[s_registry.tabCount - 1]);
        s_registry.tabCount--;
        return true;
    }

    /// @brief Override the icon slots for a registered tab.
    /// @param tabIndex Tab index from Register() or FindById().
    /// @param selectedSlot HUD icon slot when selected.
    /// @param deselectedSlot HUD icon slot when deselected.
    /// @return True on success.
    bool SetIconSlots(int tabIndex, int selectedSlot, int deselectedSlot)
    {
        if (tabIndex < 0 || tabIndex >= s_registry.tabCount) {
            return false;
        }

        s_registry.tabList[tabIndex].hudSel = selectedSlot;
        s_registry.tabList[tabIndex].hudDesel = deselectedSlot;
        return true;
    }

} // namespace Tabs
