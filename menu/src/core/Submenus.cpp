/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */

#include "Submenus.hpp"
#include "SubmenuApi.hpp"
#include "Tabs.hpp"
#include "Gfx.hpp"
#include "Icons.hpp"
#include "App.hpp"
#include "Log.hpp"
#include "Touch.hpp"
#include <dlink/dlink.h>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

namespace Submenus {

    namespace {

        constexpr char kNekoMenuBase[] = "sdmc:/qlaunch-ext/NekoMenu";
        constexpr int kSubmenuMax = 7;
        constexpr int kSubmenuFileCap = 63;
        constexpr int kSubmenuScanCap = 16;
        constexpr int kSubmenuDirMax = 160;

        enum class SubmenuPhase {
            Empty,
            Loading,
            Ready,
        };

        struct SubmenuDrawApi {
            void (*draw)(const MenuHostApi *hostApi);
            void (*input)(const MenuHostApi *hostApi, unsigned long long buttonsDown, unsigned long long buttonsHeld);
            void (*open)(const MenuHostApi *hostApi);
            void (*close)(const MenuHostApi *hostApi);
            void (*tick)(const MenuHostApi *hostApi);
            void (*shutdown)(void);
            int (*accentColor)(const MenuHostApi *hostApi, float *redOut, float *greenOut, float *blueOut);
            int (*backgroundColor)(const MenuHostApi *hostApi, float *redOut, float *greenOut, float *blueOut);
            int (*isFull)(void);
            int (*inputLocked)(void);
            void (*refresh)(void);
            int (*drag)(float moveX, float moveY, int phase);
            int (*tap)(float x, float y);
            int (*touchAction)(int cellIndex);
            int (*touchDown)(float x, float y);
            int (*touchUp)(float x, float y);
        };

        struct SubmenuInstance {
            char fileName[kSubmenuFileCap + 1];
            char tabId[kTabIdMax];
            void *handle;
            SubmenuDrawApi callbacks;
            int iconSlot;
            int selectedIconSlot;
            SubmenuPhase phase;
        };

        static SubmenuInstance s_instances[kSubmenuMax];
        static int s_instanceCount = 0;
        static const MenuHostApi *s_hostApi = nullptr;
        static bool s_booted = false;
        static char s_submenuDir[kSubmenuDirMax] = { 0 };

        /// @brief Whether a suffix is dnro (case-insensitive).
        /// @param suffix Four-char suffix without dot.
        /// @return True for dnro.
        bool IsDnroSuffix(const char *suffix)
        {
            char first = *suffix;
            return (first == 'd' || first == 'D') && (suffix[1] == 'n' || suffix[1] == 'N') &&
                   (suffix[2] == 'r' || suffix[2] == 'R') && (suffix[3] == 'o' || suffix[3] == 'O') &&
                   suffix[4] == 0;
        }

        /// @brief Whether a file name ends with .dnro.
        /// @param fileName File name.
        /// @return True for dnro files.
        bool HasDnroSuffix(const char *fileName)
        {
            size_t nameLength = strlen(fileName);

            if (nameLength < 6 || nameLength > (size_t)kSubmenuFileCap) {
                return false;
            }

            if (fileName[nameLength - 5] != '.') {
                return false;
            }

            return IsDnroSuffix(fileName + nameLength - 4);
        }

        /// @brief Whether a file name is safe to join.
        /// @param fileName File name.
        /// @return True when safe.
        bool IsFileNameSafe(const char *fileName)
        {
            if (!fileName || !fileName[0]) {
                return false;
            }

            for (const char *cursor = fileName; *cursor; cursor++) {
                char current = *cursor;

                if (current == '/' || current == '\\' || current == '"' || current == ':') {
                    return false;
                }

                if ((unsigned char)current < 32) {
                    return false;
                }
            }

            if (strstr(fileName, "..") != nullptr) {
                return false;
            }

            return true;
        }

        /// @brief Copy a submenu file name with truncation safety.
        /// @param destinationBuffer Destination buffer.
        /// @param sourceText Source name.
        /// @return True when the whole name fit.
        bool CopyFileName(char *destinationBuffer, const char *sourceText)
        {
            if (!destinationBuffer || !sourceText) {
                return false;
            }

            size_t sourceLength = strlen(sourceText);

            if (sourceLength == 0 || sourceLength > (size_t)kSubmenuFileCap) {
                return false;
            }

            memcpy(destinationBuffer, sourceText, sourceLength + 1);
            return true;
        }

        /// @brief Join the submenu dir and a file name into a path.
        /// @param outputBuffer Output buffer.
        /// @param outputCapacity Buffer capacity.
        /// @param fileName Submenu file name.
        /// @return True when the whole path fit.
        bool JoinSubmenuPath(char *outputBuffer, unsigned outputCapacity, const char *fileName)
        {
            if (!outputBuffer || outputCapacity == 0 || !fileName) {
                return false;
            }

            size_t dirLength = strlen(s_submenuDir);
            size_t nameLength = strlen(fileName);

            if (nameLength == 0 || nameLength > (size_t)kSubmenuFileCap) {
                return false;
            }

            if (dirLength + 1 + nameLength + 1 > outputCapacity) {
                return false;
            }

            memcpy(outputBuffer, s_submenuDir, dirLength);
            outputBuffer[dirLength] = '/';
            memcpy(outputBuffer + dirLength + 1, fileName, nameLength + 1);
            return true;
        }

        /// @brief Scan the submenu directory for dnro files, sorted.
        /// @param fileNames Output file names.
        /// @param fileCapacity Table capacity.
        /// @return File count.
        int ScanSubmenuDir(char fileNames[][kSubmenuFileCap + 1], int fileCapacity)
        {
            int fileCount = 0;
            DIR *directory = opendir(s_submenuDir);

            if (!directory) {
                return 0;
            }

            for (struct dirent *entry = readdir(directory); entry; entry = readdir(directory)) {
                if (fileCount >= fileCapacity) {
                    break;
                }

                if (entry->d_name[0] == '.') {
                    continue;
                }

                if (!HasDnroSuffix(entry->d_name)) {
                    continue;
                }

                if (!IsFileNameSafe(entry->d_name)) {
                    continue;
                }

                if (!CopyFileName(fileNames[fileCount], entry->d_name)) {
                    continue;
                }

                fileCount++;
            }

            closedir(directory);

            for (int sortedEnd = 1; sortedEnd < fileCount; sortedEnd++) {
                char sortKey[kSubmenuFileCap + 1];
                CopyFileName(sortKey, fileNames[sortedEnd]);
                int sortIndex = sortedEnd - 1;

                while (sortIndex >= 0 && strcmp(fileNames[sortIndex], sortKey) > 0) {
                    CopyFileName(fileNames[sortIndex + 1], fileNames[sortIndex]);
                    sortIndex--;
                }

                CopyFileName(fileNames[sortIndex + 1], sortKey);
            }

            return fileCount;
        }

        /// @brief Whether a submenu id is usable.
        /// @param tabId Lowercase id.
        /// @return True when valid.
        bool IsTabIdUsable(const char *tabId)
        {
            if (!tabId || !tabId[0]) {
                return false;
            }

            size_t idLength = strlen(tabId);

            if (idLength >= (size_t)kTabIdMax) {
                return false;
            }

            for (size_t charIndex = 0; charIndex < idLength; charIndex++) {
                char current = tabId[charIndex];
                bool lower = current >= 'a' && current <= 'z';
                bool digit = current >= '0' && current <= '9';

                if (!lower && !digit && current != '_') {
                    return false;
                }
            }

            return true;
        }

        /// @brief Find a loaded instance by tab id.
        /// @param tabId Tab id.
        /// @return Instance or null.
        SubmenuInstance *FindInstanceByTabId(const char *tabId)
        {
            if (!tabId) {
                return nullptr;
            }

            for (int instanceIndex = 0; instanceIndex < s_instanceCount; instanceIndex++) {
                if (strcmp(s_instances[instanceIndex].tabId, tabId) == 0) {
                    return &s_instances[instanceIndex];
                }
            }

            return nullptr;
        }

        /// @brief Find a loaded instance by registry index.
        /// @param tabIndex Tabs registry index.
        /// @return Instance or null.
        SubmenuInstance *FindInstanceByTabIndex(int tabIndex)
        {
            const MenuTab *tabDescriptor = Tabs::Get(tabIndex);

            if (!tabDescriptor) {
                return nullptr;
            }

            return FindInstanceByTabId(tabDescriptor->id);
        }

        /// @brief Upload a submenu icon PNG to a texture slot.
        /// @param pngBytes Embedded PNG bytes.
        /// @param pngSize PNG byte size.
        /// @return Texture slot or -1.
        int UploadSubmenuIcon(const unsigned char *pngBytes, unsigned pngSize)
        {
            int textureSlot = -1;

            if (Icons::UploadPng(pngBytes, pngSize, true, &textureSlot)) {
                return textureSlot;
            }

            return -1;
        }

        /// @brief Load one submenu dnro and register its tab.
        /// @param fileName Submenu file name.
        /// @return True on success.
        bool LoadOneSubmenu(const char *fileName)
        {
            if (s_instanceCount >= kSubmenuMax) {
                Log::Line("[submenus] table full, skipping %s", fileName);
                return false;
            }

            char filePath[160];

            if (!JoinSubmenuPath(filePath, sizeof(filePath), fileName)) {
                return false;
            }

            void *handle = dlopen(filePath, RTLD_LOCAL);

            if (!handle) {
                Log::Line("[submenus] cannot open %s", fileName);
                return false;
            }

            int (*apiVersion)(void) = (int (*)(void))dlsym(handle, "SubmenuApiVersion");
            const char *(*submenuId)(void) = (const char *(*)(void))dlsym(handle, "SubmenuId");
            const char *(*submenuTitle)(void) = (const char *(*)(void))dlsym(handle, "SubmenuTitle");
            int (*submenuBoot)(const MenuHostApi *) = (int (*)(const MenuHostApi *))dlsym(handle, "SubmenuBoot");
            void (*submenuDraw)(const MenuHostApi *) = (void (*)(const MenuHostApi *))dlsym(handle, "SubmenuDraw");

            if (!apiVersion || !submenuId || !submenuTitle || !submenuBoot || !submenuDraw) {
                Log::Line("[submenus] %s misses required exports", fileName);
                dlclose(handle);
                return false;
            }

            if (apiVersion() != kSubmenuApiVersion) {
                Log::Line("[submenus] %s bad api version", fileName);
                dlclose(handle);
                return false;
            }

            const char *tabId = submenuId();
            const char *tabTitle = submenuTitle();

            if (!IsTabIdUsable(tabId) || !tabTitle || !tabTitle[0]) {
                Log::Line("[submenus] %s bad id/title", fileName);
                dlclose(handle);
                return false;
            }

            if (Tabs::FindById(tabId) >= 0 || FindInstanceByTabId(tabId)) {
                Log::Line("[submenus] %s duplicate id", fileName);
                dlclose(handle);
                return false;
            }

            if (submenuBoot(s_hostApi) != 0) {
                Log::Line("[submenus] %s boot rejected", fileName);
                dlclose(handle);
                return false;
            }

            const char *(*submenuJingle)(void) = (const char *(*)(void))dlsym(handle, "SubmenuJingle");
            unsigned (*submenuGlyph)(void) = (unsigned (*)(void))dlsym(handle, "SubmenuGlyph");
            const unsigned char *(*submenuIcon)(unsigned *) = (const unsigned char *(*)(unsigned *))dlsym(handle, "SubmenuIconPng");

            MenuTab tabDescriptor;
            memset(&tabDescriptor, 0, sizeof(tabDescriptor));
            snprintf(tabDescriptor.id, sizeof(tabDescriptor.id), "%s", tabId);
            snprintf(tabDescriptor.title, sizeof(tabDescriptor.title), "%s", tabTitle);
            tabDescriptor.id[sizeof(tabDescriptor.id) - 1] = 0;
            tabDescriptor.title[sizeof(tabDescriptor.title) - 1] = 0;
            tabDescriptor.glyph = submenuGlyph ? submenuGlyph() : 0;
            tabDescriptor.hudSel = -1;
            tabDescriptor.hudDesel = -1;

            if (submenuJingle) {
                const char *jingleName = submenuJingle();

                if (jingleName && jingleName[0]) {
                    snprintf(tabDescriptor.jingle, sizeof(tabDescriptor.jingle), "%s", jingleName);
                }
            }

            tabDescriptor.draw = nullptr;
            tabDescriptor.input = nullptr;
            tabDescriptor.onEnter = nullptr;
            tabDescriptor.onExit = nullptr;

            int tabIndex = Tabs::Register(tabDescriptor);

            if (tabIndex < 0) {
                Log::Line("[submenus] %s registry full", fileName);
                void (*submenuShutdown)(void) = (void (*)(void))dlsym(handle, "SubmenuShutdown");

                if (submenuShutdown) {
                    submenuShutdown();
                }

                dlclose(handle);
                return false;
            }

            int iconSlot = -1;
            int selectedIconSlot = -1;

            if (submenuIcon) {
                unsigned pngSize = 0;
                const unsigned char *pngBytes = submenuIcon(&pngSize);
                iconSlot = UploadSubmenuIcon(pngBytes, pngSize);
            }

            const unsigned char *(*submenuSelectedIcon)(unsigned *) = (const unsigned char *(*)(unsigned *))dlsym(handle, "SubmenuSelectedIconPng");

            if (submenuSelectedIcon) {
                unsigned selectedPngSize = 0;
                const unsigned char *selectedPngBytes = submenuSelectedIcon(&selectedPngSize);
                selectedIconSlot = UploadSubmenuIcon(selectedPngBytes, selectedPngSize);
            }

            if (iconSlot < 0) {
                iconSlot = selectedIconSlot;
            }

            if (selectedIconSlot < 0) {
                selectedIconSlot = iconSlot;
            }

            if (iconSlot >= 0) {
                Tabs::SetIconSlots(tabIndex, iconSlot, selectedIconSlot);
            }

            SubmenuInstance *instance = &s_instances[s_instanceCount++];
            memset(instance, 0, sizeof(*instance));
            CopyFileName(instance->fileName, fileName);
            snprintf(instance->tabId, sizeof(instance->tabId), "%s", tabId);
            instance->handle = handle;
            instance->callbacks.draw = submenuDraw;
            instance->callbacks.input = (void (*)(const MenuHostApi *, unsigned long long, unsigned long long))dlsym(handle, "SubmenuInput");
            instance->callbacks.open = (void (*)(const MenuHostApi *))dlsym(handle, "SubmenuOpen");
            instance->callbacks.close = (void (*)(const MenuHostApi *))dlsym(handle, "SubmenuClose");
            instance->callbacks.tick = (void (*)(const MenuHostApi *))dlsym(handle, "SubmenuTick");
            instance->callbacks.shutdown = (void (*)(void))dlsym(handle, "SubmenuShutdown");
            instance->callbacks.accentColor = (int (*)(const MenuHostApi *, float *, float *, float *))dlsym(handle, "SubmenuColor");
            instance->callbacks.backgroundColor = (int (*)(const MenuHostApi *, float *, float *, float *))dlsym(handle, "SubmenuBackgroundColor");
            instance->callbacks.isFull = (int (*)(void))dlsym(handle, "SubmenuIsFull");
            instance->callbacks.inputLocked = (int (*)(void))dlsym(handle, "SubmenuInputLocked");
            instance->callbacks.refresh = (void (*)(void))dlsym(handle, "SubmenuRefresh");
            instance->callbacks.drag = (int (*)(float, float, int))dlsym(handle, "SubmenuDrag");
            instance->callbacks.tap = (int (*)(float, float))dlsym(handle, "SubmenuTap");
            instance->callbacks.touchAction = (int (*)(int))dlsym(handle, "SubmenuTouchAction");
            instance->callbacks.touchDown = (int (*)(float, float))dlsym(handle, "SubmenuTouchDown");
            instance->callbacks.touchUp = (int (*)(float, float))dlsym(handle, "SubmenuTouchUp");
            instance->iconSlot = iconSlot;
            instance->selectedIconSlot = selectedIconSlot;
            instance->phase = SubmenuPhase::Ready;
            Log::Line("[submenus] loaded %s as %s (icon %d/%d, color %d)", fileName, tabId,
                      iconSlot, selectedIconSlot, instance->callbacks.accentColor != nullptr);
            return true;
        }

    } // namespace

    /// @brief Boot dynamic submenus from a theme pack directory.
    /// @param hostApi Host table with menu functions plus qext.
    /// @param submenuDir Pack submenus dir (for example "sdmc:/qlaunch-ext/NekoMenu/default/submenus").
    void Boot(const MenuHostApi *hostApi, const char *submenuDir)
    {
        if (s_booted) {
            return;
        }

        if (!submenuDir || !submenuDir[0] || strlen(submenuDir) >= (size_t)kSubmenuDirMax) {
            return;
        }

        s_booted = true;
        s_hostApi = hostApi;
        s_instanceCount = 0;
        snprintf(s_submenuDir, sizeof(s_submenuDir), "%s", submenuDir);
        s_submenuDir[sizeof(s_submenuDir) - 1] = 0;

        mkdir(kNekoMenuBase, 0777);
        mkdir(s_submenuDir, 0777);

        char fileNames[kSubmenuScanCap][kSubmenuFileCap + 1];
        int fileCount = ScanSubmenuDir(fileNames, kSubmenuScanCap);
        Log::Line("[submenus] found %d files", fileCount);

        for (int fileIndex = 0; fileIndex < fileCount; fileIndex++) {
            LoadOneSubmenu(fileNames[fileIndex]);
        }
    }

    /// @brief Shut down submenus, freeing icons and unloading dnro files.
    void Shutdown()
    {
        if (!s_booted) {
            return;
        }

        for (int instanceIndex = s_instanceCount - 1; instanceIndex >= 0; instanceIndex--) {
            SubmenuInstance *instance = &s_instances[instanceIndex];

            if (instance->callbacks.shutdown) {
                instance->callbacks.shutdown();
            }

            if (instance->selectedIconSlot >= 0 && instance->selectedIconSlot != instance->iconSlot) {
                Gfx::TexFree(instance->selectedIconSlot);
            }

            instance->selectedIconSlot = -1;

            if (instance->iconSlot >= 0) {
                Gfx::TexFree(instance->iconSlot);
                instance->iconSlot = -1;
            }

            int tabIndex = Tabs::FindById(instance->tabId);

            if (tabIndex >= 0) {
                Tabs::RemoveAt(tabIndex);
            }

            if (instance->handle) {
                dlclose(instance->handle);
                instance->handle = nullptr;
            }

            memset(instance, 0, sizeof(*instance));
        }

        s_instanceCount = 0;
        s_hostApi = nullptr;
        s_booted = false;
        s_submenuDir[0] = 0;
    }

    /// @brief Tick loaded submenus once per frame.
    void Tick()
    {
        if (!s_booted || !s_hostApi) {
            return;
        }

        for (int instanceIndex = 0; instanceIndex < s_instanceCount; instanceIndex++) {
            if (s_instances[instanceIndex].phase != SubmenuPhase::Ready) {
                continue;
            }

            if (s_instances[instanceIndex].callbacks.tick) {
                s_instances[instanceIndex].callbacks.tick(s_hostApi);
            }
        }
    }

    /// @brief Whether a tab index belongs to a dynamic submenu.
    /// @param tabIndex Tabs registry index.
    /// @return True for dynamic submenu tabs.
    bool IsDynamicTab(int tabIndex)
    {
        return FindInstanceByTabIndex(tabIndex) != nullptr;
    }

    /// @brief Draw a dynamic submenu tab onto the core state.
    /// @param tabIndex Tabs registry index.
    void DrawDynamicTab(int tabIndex)
    {
        SubmenuInstance *instance = FindInstanceByTabIndex(tabIndex);

        if (!instance || !s_hostApi) {
            return;
        }

        if (instance->phase != SubmenuPhase::Ready || !instance->callbacks.draw) {
            return;
        }

        Touch::BeginTab(tabIndex);
        instance->callbacks.draw(s_hostApi);
    }

    /// @brief Handle input for a dynamic submenu tab.
    /// @param tabIndex Tabs registry index.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    void InputDynamicTab(int tabIndex, unsigned long long buttonsDown, unsigned long long buttonsHeld)
    {
        SubmenuInstance *instance = FindInstanceByTabIndex(tabIndex);

        if (!instance || !s_hostApi) {
            return;
        }

        if (instance->phase != SubmenuPhase::Ready) {
            return;
        }

        if (!instance->callbacks.input) {
            if (buttonsDown & (u64)HidNpadButton_B) {
                App::SwitchMenuById("home");
            }

            return;
        }

        instance->callbacks.input(s_hostApi, buttonsDown, buttonsHeld);
    }

    /// @brief Act on a registered touch cell (select, re-tap acts).
    /// @param tabIndex Tabs registry index.
    /// @param cellIndex Cell index from touchRegisterCell.
    /// @return Nonzero when the tap was consumed.
    int TouchAction(int tabIndex, int cellIndex)
    {
        SubmenuInstance *instance = FindInstanceByTabIndex(tabIndex);

        if (!instance || instance->phase != SubmenuPhase::Ready || !instance->callbacks.touchAction) {
            return 0;
        }

        return instance->callbacks.touchAction(cellIndex) != 0 ? 1 : 0;
    }

    /// @brief Drag-scroll submenu content with inertia on release.
    /// @param tabIndex Tabs registry index.
    /// @param moveX Horizontal finger travel this frame.
    /// @param moveY Vertical finger travel this frame.
    /// @param phase Touch phase (held moves, up releases).
    /// @return Nonzero when the drag was consumed.
    int Drag(int tabIndex, float moveX, float moveY, int phase)
    {
        SubmenuInstance *instance = FindInstanceByTabIndex(tabIndex);

        if (!instance || instance->phase != SubmenuPhase::Ready || !instance->callbacks.drag) {
            return 0;
        }

        return instance->callbacks.drag(moveX, moveY, phase) != 0 ? 1 : 0;
    }

    /// @brief Deliver one tap to a submenu tab (select, double-tap runs).
    /// @param tabIndex Tabs registry index.
    /// @param x Tap X in design space.
    /// @param y Tap Y in design space.
    /// @return Nonzero when the tap was consumed.
    int Tap(int tabIndex, float x, float y)
    {
        SubmenuInstance *instance = FindInstanceByTabIndex(tabIndex);

        if (!instance || instance->phase != SubmenuPhase::Ready || !instance->callbacks.tap) {
            return 0;
        }

        return instance->callbacks.tap(x, y) != 0 ? 1 : 0;
    }

    /// @brief Deliver a finger-down press to a submenu tab.
    /// @param tabIndex Tabs registry index.
    /// @param x Down X in design space.
    /// @param y Down Y in design space.
    /// @return Nonzero when the submenu claimed the finger.
    int TouchDown(int tabIndex, float x, float y)
    {
        SubmenuInstance *instance = FindInstanceByTabIndex(tabIndex);

        if (!instance || instance->phase != SubmenuPhase::Ready || !instance->callbacks.touchDown) {
            return 0;
        }

        return instance->callbacks.touchDown(x, y) != 0 ? 1 : 0;
    }

    /// @brief Deliver a claimed finger release to a submenu tab.
    /// @param tabIndex Tabs registry index.
    /// @param x Up X in design space.
    /// @param y Up Y in design space.
    /// @return Nonzero when the release was consumed.
    int TouchUp(int tabIndex, float x, float y)
    {
        SubmenuInstance *instance = FindInstanceByTabIndex(tabIndex);

        if (!instance || instance->phase != SubmenuPhase::Ready || !instance->callbacks.touchUp) {
            return 0;
        }

        return instance->callbacks.touchUp(x, y) != 0 ? 1 : 0;
    }

    /// @brief Enter a dynamic submenu tab.
    /// @param tabIndex Tabs registry index.
    void EnterDynamicTab(int tabIndex)
    {
        SubmenuInstance *instance = FindInstanceByTabIndex(tabIndex);

        if (!instance || !s_hostApi) {
            return;
        }

        if (instance->phase != SubmenuPhase::Ready || !instance->callbacks.open) {
            return;
        }

        instance->callbacks.open(s_hostApi);
    }

    /// @brief Exit a dynamic submenu tab.
    /// @param tabIndex Tabs registry index.
    void ExitDynamicTab(int tabIndex)
    {
        SubmenuInstance *instance = FindInstanceByTabIndex(tabIndex);

        if (!instance || !s_hostApi) {
            return;
        }

        if (instance->phase != SubmenuPhase::Ready || !instance->callbacks.close) {
            return;
        }

        instance->callbacks.close(s_hostApi);
    }

    /// @brief Give a dynamic submenu tab accent color.
    /// @param tabIndex Tabs registry index.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    /// @return True when the submenu provides a color.
    bool TabColor(int tabIndex, float *redOut, float *greenOut, float *blueOut)
    {
        SubmenuInstance *instance = FindInstanceByTabIndex(tabIndex);

        if (!instance || !s_hostApi || !redOut || !greenOut || !blueOut) {
            return false;
        }

        if (instance->phase != SubmenuPhase::Ready || !instance->callbacks.accentColor) {
            return false;
        }

        return instance->callbacks.accentColor(s_hostApi, redOut, greenOut, blueOut) != 0;
    }

    /// @brief Give a dynamic submenu tab background color.
    /// @param tabIndex Tabs registry index.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    /// @return True when the submenu provides a color.
    bool TabBackgroundColor(int tabIndex, float *redOut, float *greenOut, float *blueOut)
    {
        SubmenuInstance *instance = FindInstanceByTabIndex(tabIndex);

        if (!instance || !s_hostApi || !redOut || !greenOut || !blueOut) {
            return false;
        }

        if (instance->phase != SubmenuPhase::Ready || !instance->callbacks.backgroundColor) {
            return false;
        }

        return instance->callbacks.backgroundColor(s_hostApi, redOut, greenOut, blueOut) != 0;
    }

    /// @brief Whether a dynamic submenu tab covers the screen.
    /// @param tabIndex Tabs registry index.
    /// @return True when fullscreen photo or video is showing.
    bool IsFullTab(int tabIndex)
    {
        SubmenuInstance *instance = FindInstanceByTabIndex(tabIndex);

        if (!instance || instance->phase != SubmenuPhase::Ready || !instance->callbacks.isFull) {
            return false;
        }

        return instance->callbacks.isFull() != 0;
    }

    /// @brief Find the covering tab, if any.
    /// @return Tabs registry index covering the screen, or -1.
    int FullTabIndex()
    {
        for (int instanceIndex = 0; instanceIndex < s_instanceCount; instanceIndex++) {
            if (s_instances[instanceIndex].phase != SubmenuPhase::Ready ||
                !s_instances[instanceIndex].callbacks.isFull) {
                continue;
            }

            if (s_instances[instanceIndex].callbacks.isFull() == 0) {
                continue;
            }

            return Tabs::FindById(s_instances[instanceIndex].tabId);
        }

        return -1;
    }

    /// @brief Whether any covering tab is showing.
    /// @return True when a fullscreen photo or video is showing.
    bool AnyFullOpen()
    {
        return FullTabIndex() >= 0;
    }

    /// @brief Close every covering viewer.
    void CloseFullTabs()
    {
        if (!s_hostApi) {
            return;
        }

        for (int instanceIndex = 0; instanceIndex < s_instanceCount; instanceIndex++) {
            if (s_instances[instanceIndex].phase != SubmenuPhase::Ready ||
                !s_instances[instanceIndex].callbacks.isFull ||
                !s_instances[instanceIndex].callbacks.close) {
                continue;
            }

            if (s_instances[instanceIndex].callbacks.isFull() != 0) {
                s_instances[instanceIndex].callbacks.close(s_hostApi);
            }
        }
    }

    /// @brief Find the tab that wants all input.
    /// @return Tabs registry index wanting input lock, or -1.
    int InputLockedTabIndex()
    {
        for (int instanceIndex = 0; instanceIndex < s_instanceCount; instanceIndex++) {
            if (s_instances[instanceIndex].phase != SubmenuPhase::Ready ||
                !s_instances[instanceIndex].callbacks.inputLocked) {
                continue;
            }

            if (s_instances[instanceIndex].callbacks.inputLocked() == 0) {
                continue;
            }

            return Tabs::FindById(s_instances[instanceIndex].tabId);
        }

        return -1;
    }

    /// @brief Drop cached textures in every tab after an external refresh.
    void RefreshAll()
    {
        for (int instanceIndex = 0; instanceIndex < s_instanceCount; instanceIndex++) {
            if (s_instances[instanceIndex].phase != SubmenuPhase::Ready ||
                !s_instances[instanceIndex].callbacks.refresh) {
                continue;
            }

            s_instances[instanceIndex].callbacks.refresh();
        }
    }

} /* namespace Submenus */
