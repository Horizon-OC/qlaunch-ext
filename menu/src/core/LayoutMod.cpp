/* qlaunch-ext layout module host (C) 2026 Souldbminer */
/* Licensed under the GPLv2                             */

#include "LayoutMod.hpp"
#include "LayoutApi.hpp"
#include "Layout.hpp"
#include "HostApi.hpp"
#include "Submenus.hpp"
#include "Packs.hpp"
#include "Touch.hpp"
#include "Log.hpp"
#include <dlink/dlink.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static_assert(sizeof(Touch::TouchPoint) == sizeof(LayoutTouch), "Touch frame ABI mismatch");
static_assert(offsetof(Touch::TouchPoint, x) == offsetof(LayoutTouch, x), "Touch frame ABI mismatch");
static_assert(offsetof(Touch::TouchPoint, y) == offsetof(LayoutTouch, y), "Touch frame ABI mismatch");
static_assert(offsetof(Touch::TouchPoint, fingerId) == offsetof(LayoutTouch, fingerId), "Touch frame ABI mismatch");
static_assert(offsetof(Touch::TouchPoint, phase) == offsetof(LayoutTouch, phase), "Touch frame ABI mismatch");

namespace LayoutMod {

    namespace {

        /// @brief Pack-relative layout module file name.
        constexpr char kLayoutFile[] = "layout.dnro";

        /// @brief Pack-relative submenu directory name.
        constexpr char kSubmenuDirName[] = "submenus";

        /// @brief Loaded layout module state, single global.
        struct LayoutModState {
            void *handle;
            void (*draw)(const MenuHostApi *);
            void (*input)(const MenuHostApi *, unsigned long long, unsigned long long,
                          const LayoutTouch *, int);
            float (*shiftX)(const MenuHostApi *);
            int (*findNode)(const MenuHostApi *, const char *, LayoutNode *);
            int (*tabCount)(const MenuHostApi *);
            int (*tabAt)(const MenuHostApi *, int);
            int (*slotOfTab)(const MenuHostApi *, int);
            void (*shutdown)(const MenuHostApi *);
            bool ready;
        };

        static LayoutModState s_state = {};

    } // namespace

    /// @brief Boot the active theme pack: its submenus, then its layout module.
    /// A design without submenus boots the layout alone.
    /// @return True when the layout (and its submenus, if any) loaded.
    bool Boot()
    {
        Shutdown();

        char packDir[160] = { 0 };

        if (!Packs::Resolve(packDir, sizeof(packDir))) {
            Log::Line("[layoutmod] cannot resolve theme pack");
            return false;
        }

        mkdir(packDir, 0777);

        char submenuDir[192] = { 0 };
        snprintf(submenuDir, sizeof(submenuDir), "%s/%s", packDir, kSubmenuDirName);
        submenuDir[sizeof(submenuDir) - 1] = 0;

        char layoutPath[192] = { 0 };
        snprintf(layoutPath, sizeof(layoutPath), "%s/%s", packDir, kLayoutFile);
        layoutPath[sizeof(layoutPath) - 1] = 0;

        s_state.handle = dlopen(layoutPath, RTLD_LOCAL);

        if (!s_state.handle) {
            Log::Line("[layoutmod] cannot open layout module");
            return false;
        }

        int (*apiVersion)(void) = (int (*)(void))dlsym(s_state.handle, "LayoutApiVersion");

        if (!apiVersion || apiVersion() != kLayoutApiVersion) {
            Log::Line("[layoutmod] bad layout api version");
            dlclose(s_state.handle);
            s_state.handle = nullptr;
            return false;
        }

        int (*layoutBoot)(const MenuHostApi *, const char *) =
            (int (*)(const MenuHostApi *, const char *))dlsym(s_state.handle, "LayoutBoot");
        s_state.draw = (void (*)(const MenuHostApi *))dlsym(s_state.handle, "LayoutDraw");
        s_state.input = (void (*)(const MenuHostApi *, unsigned long long, unsigned long long,
                                  const LayoutTouch *, int))dlsym(s_state.handle, "LayoutInput");
        s_state.shiftX = (float (*)(const MenuHostApi *))dlsym(s_state.handle, "LayoutShiftX");
        s_state.findNode = (int (*)(const MenuHostApi *, const char *, LayoutNode *))dlsym(s_state.handle, "LayoutFindNode");
        s_state.tabCount = (int (*)(const MenuHostApi *))dlsym(s_state.handle, "LayoutTabCount");
        s_state.tabAt = (int (*)(const MenuHostApi *, int))dlsym(s_state.handle, "LayoutTabAt");
        s_state.slotOfTab = (int (*)(const MenuHostApi *, int))dlsym(s_state.handle, "LayoutSlotOfTab");
        s_state.shutdown = (void (*)(const MenuHostApi *))dlsym(s_state.handle, "LayoutShutdown");

        if (!layoutBoot || !s_state.draw || !s_state.input || !s_state.shiftX || !s_state.findNode || !s_state.tabCount || !s_state.tabAt || !s_state.slotOfTab || !s_state.shutdown) {
            Log::Line("[layoutmod] layout module missing entry points");
            dlclose(s_state.handle);
            s_state.handle = nullptr;
            return false;
        }

        const MenuHostApi *hostApi = HostApi::GetHostApi();
        Submenus::Boot(hostApi, submenuDir);

        if (layoutBoot(hostApi, packDir) != 0) {
            Log::Line("[layoutmod] layout boot rejected");
            Submenus::Shutdown();
            dlclose(s_state.handle);
            s_state.handle = nullptr;
            return false;
        }

        s_state.ready = true;
        return true;
    }

    /// @brief Shut down submenus, then unload the layout module.
    void Shutdown()
    {
        Submenus::Shutdown();

        if (s_state.handle) {
            if (s_state.ready && s_state.shutdown) {
                s_state.shutdown(HostApi::GetHostApi());
            }

            dlclose(s_state.handle);
        }

        s_state.handle = nullptr;
        s_state.draw = nullptr;
        s_state.input = nullptr;
        s_state.shiftX = nullptr;
        s_state.findNode = nullptr;
        s_state.tabCount = nullptr;
        s_state.tabAt = nullptr;
        s_state.slotOfTab = nullptr;
        s_state.shutdown = nullptr;
        s_state.ready = false;
    }

    /// @brief Draw chrome plus the active tab (dropped without layout).
    void Draw()
    {
        if (!s_state.ready || !s_state.draw) {
            return;
        }

        s_state.draw(HostApi::GetHostApi());
    }

    /// @brief Route input to chrome or the active tab (dropped without layout).
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    void Input(u64 buttonsDown, u64 buttonsHeld)
    {
        if (!s_state.ready || !s_state.input) {
            return;
        }

        const Touch::TouchFrame &touchFrame = Touch::Frame();
        s_state.input(HostApi::GetHostApi(), buttonsDown, buttonsHeld,
                      reinterpret_cast<const LayoutTouch *>(touchFrame.points), touchFrame.count);
    }

    /// @brief Give the layout slide offset (0 without layout).
    /// @return Slide offset in pixels.
    float ShiftX()
    {
        if (!s_state.ready || !s_state.shiftX) {
            return 0.0f;
        }

        return s_state.shiftX(HostApi::GetHostApi());
    }

    /// @brief Find a layout node by type (false without layout).
    /// @param type Node type string.
    /// @param nodeOut Output node (copied).
    /// @return True when found.
    bool FindNode(const char *type, LayoutNode *nodeOut)
    {
        if (!s_state.ready || !s_state.findNode || !type || !nodeOut) {
            return false;
        }

        return s_state.findNode(HostApi::GetHostApi(), type, nodeOut) != 0;
    }

    /// @brief Give the visible tab slot count (0 without layout).
    /// @return Slot count.
    int TabCount()
    {
        if (!s_state.ready || !s_state.tabCount) {
            return 0;
        }

        return s_state.tabCount(HostApi::GetHostApi());
    }

    /// @brief Resolve a visible slot to a Tabs registry index.
    /// @param slot Layout order 0..TabCount-1.
    /// @return Tabs index, or -1 when unresolvable.
    int TabAt(int slot)
    {
        if (!s_state.ready || !s_state.tabAt) {
            return -1;
        }

        return s_state.tabAt(HostApi::GetHostApi(), slot);
    }

    /// @brief Find the visible slot for a Tabs registry index.
    /// @param tab Tabs index.
    /// @return Layout slot, or -1 when hidden.
    int SlotOfTab(int tab)
    {
        if (!s_state.ready || !s_state.slotOfTab) {
            return -1;
        }

        return s_state.slotOfTab(HostApi::GetHostApi(), tab);
    }

} /* namespace LayoutMod */
