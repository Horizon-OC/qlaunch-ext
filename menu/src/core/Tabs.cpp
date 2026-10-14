/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Tabs.hpp"
#include "../widgets/Widgets.hpp"
#include <cstring>

namespace Tabs {

    namespace {

        /// @brief Backing store for the tab registry.
        MenuTab s_tabs[TAB_MAX];

        /// @brief Number of live entries in s_tabs.
        int s_count = 0;

        /// @brief Fill a MenuTab with safe defaults.
        /// @param out Tab to initialize.
        void ClearTab(MenuTab &out)
        {
            memset(&out, 0, sizeof(out));
            out.glyph = 0;
            out.hudSel = -1;
            out.hudDesel = -1;
            out.jingle[0] = 0;
            out.draw = NULL;
            out.input = NULL;
            out.onEnter = NULL;
            out.onExit = NULL;
        }

        /// @brief Copy a C string with truncation safety.
        /// @param out Destination buffer.
        /// @param outcap Size of the destination buffer.
        /// @param src Source string.
        static void CopyStr(char *out, unsigned outcap, const char *src) {
            if (!out || !outcap) {
                return;
            }

            if (!src) {
                out[0] = 0;
                return;
            }

            size_t len = strlen(src);

            if (len > (size_t)(outcap - 1)) {
                len = (size_t)(outcap - 1);
            }

            memcpy(out, src, len);
            out[len] = 0;
        }

        /// @brief Copy a jingle clip name with truncation safety.
        /// @param out Destination tab receiving the jingle.
        /// @param jingle SFX clip name, or null/empty to mute.
        void CopyJingle(MenuTab &out, const char *jingle)
        {
            if (!jingle || !jingle[0]) {
                out.jingle[0] = 0;
                return;
            }

            CopyStr(out.jingle, sizeof(out.jingle), jingle);
        }

    } // namespace

    /// @brief Register a tab. Returns its index, or -1 when full.
    /// @param tab Descriptor to copy into the registry.
    /// @return Index of the tab, or -1 on failure.
    int Register(const MenuTab &tab)
    {
        if (s_count < 0 || s_count >= TAB_MAX) {
            return -1;
        }

        /* Note: draw may be null (home renders via the gamestrip node). */
        if (tab.id[0] == 0) {
            return -1;
        }

        for (int i = 0; i < s_count; i++) {
            if (strncmp(s_tabs[i].id, tab.id, TAB_ID_MAX) == 0) {
                return -1;
            }
        }

        s_tabs[s_count] = tab;
        s_tabs[s_count].id[TAB_ID_MAX - 1] = 0;
        s_tabs[s_count].title[TAB_TITLE_MAX - 1] = 0;
        return s_count++;
    }

    /// @brief Number of registered tabs.
    /// @return Tab count.
    int Count()
    {
        return s_count;
    }

    /// @brief Look up a tab by index.
    /// @param i Tab index.
    /// @return Pointer to the tab, or null when out of range.
    const MenuTab *Get(int i)
    {
        if (i < 0 || i >= s_count) {
            return NULL;
        }

        return &s_tabs[i];
    }

    /// @brief Look up a tab by string id.
    /// @param id Tab id (for example "home").
    /// @return Index of the tab, or -1 when missing.
    int FindById(const char *id)
    {
        if (!id || !id[0]) {
            return -1;
        }

        for (int i = 0; i < s_count; i++) {
            if (strncmp(s_tabs[i].id, id, TAB_ID_MAX) == 0) {
                return i;
            }
        }

        return -1;
    }

    /// @brief Override the jingle clip for a registered tab.
    /// @param i Tab index from Register() or FindById().
    /// @param jingle SFX clip name (for example "home_jingle"), or null/empty to mute.
    /// @return True on success.
    bool SetJingle(int i, const char *jingle)
    {
        if (i < 0 || i >= s_count) {
            return false;
        }

        CopyJingle(s_tabs[i], jingle);
        return true;
    }

    /// @brief Clear the registry. Used by tests and reboot.
    void Clear()
    {
        for (int i = 0; i < TAB_MAX; i++) {
            ClearTab(s_tabs[i]);
        }

        s_count = 0;
    }

    /// @brief Register the builtin tabs (home/connect/eshop/album/settings).
    void RegisterBuiltin()
    {
        Clear();

        MenuTab tab;
        ClearTab(tab);

        CopyStr(tab.id, sizeof(tab.id), "home");
        CopyStr(tab.title, sizeof(tab.title), "Home");
        tab.glyph = 0xE044;
        CopyJingle(tab, "home_jingle");
        tab.draw = NULL; /* Home draws via the gamestrip node. */
        tab.input = NULL;
        /* Home keeps a null draw/input so Layout owns the strip node. */
        Register(tab);

        ClearTab(tab);
        CopyStr(tab.id, sizeof(tab.id), "connect");
        CopyStr(tab.title, sizeof(tab.title), "Friends");
        tab.glyph = 0xE132;
        CopyJingle(tab, "chat_jingle");
        tab.draw = WConnect::Draw;
        tab.input = WConnect::Input;
        Register(tab);

        ClearTab(tab);
        CopyStr(tab.id, sizeof(tab.id), "eshop");
        CopyStr(tab.title, sizeof(tab.title), "Shop");
        tab.glyph = 0xE133;
        CopyJingle(tab, "eshop_jingle");
        tab.draw = WEShop::Draw;
        tab.input = WEShop::Input;
        Register(tab);

        ClearTab(tab);
        CopyStr(tab.id, sizeof(tab.id), "album");
        CopyStr(tab.title, sizeof(tab.title), "Album");
        tab.glyph = 0xE134;
        CopyJingle(tab, "album_jingle");
        tab.draw = WAlbum::Draw;
        tab.input = NULL; /* Album input returns bool; routed by Layout. */
        tab.onEnter = WAlbum::Open;
        tab.onExit = WAlbum::Close;
        Register(tab);

        ClearTab(tab);
        CopyStr(tab.id, sizeof(tab.id), "settings");
        CopyStr(tab.title, sizeof(tab.title), "Settings");
        tab.glyph = 0xE130;
        CopyJingle(tab, "settings_jingle");
        tab.draw = WSettings::Draw;
        tab.input = WSettings::Input;
        Register(tab);
    }

} // namespace Tabs
