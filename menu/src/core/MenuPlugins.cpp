/* qlaunch-ext menu button plugins (C) 2026 Souldbminer */
/* Licensed under the GPLv2                            */

#include "MenuPlugins.hpp"
#include "MenuPluginApi.hpp"
#include "Tabs.hpp"
#include "Icons.hpp"
#include "Gfx.hpp"
#include "Log.hpp"
#include <dlink/dlink.h>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

namespace MenuPlugins {

    namespace {

        constexpr int kPluginMax = 8;
        constexpr int kButtonMax = 16;
        constexpr int kPluginFileCap = 63;
        constexpr int kPluginScanCap = 16;
        constexpr int kPluginDirMax = 160;
        constexpr int kPluginIdMax = 63;
        constexpr int kButtonLabelCap = 48;

        enum class PluginPhase {
            Empty,
            Loading,
            Ready,
        };

        struct PluginCallbacks {
            int (*buttonCount)(void);
            const char *(*buttonTab)(int buttonIndex);
            const char *(*buttonLabel)(int buttonIndex);
            const unsigned char *(*buttonIcon)(int buttonIndex, unsigned *outputSize);
            int (*buttonActivate)(const MenuHostApi *hostApi, int buttonIndex);
            void (*shutdown)(void);
        };

        struct PluginInstance {
            char fileName[kPluginFileCap + 1];
            char pluginId[kPluginIdMax + 1];
            void *handle;
            PluginCallbacks callbacks;
            int buttonBase;
            int buttonCount;
            PluginPhase phase;
        };

        struct ButtonEntry {
            int pluginIndex;
            int pluginButton;
            int iconSlot;
            char label[kButtonLabelCap + 1];
        };

        static PluginInstance s_instances[kPluginMax];
        static int s_instanceCount = 0;
        static ButtonEntry s_buttons[kButtonMax];
        static int s_buttonCount = 0;
        static const MenuHostApi *s_hostApi = nullptr;
        static bool s_booted = false;
        static char s_pluginDir[kPluginDirMax] = { 0 };

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

            if (nameLength < 6 || nameLength > (size_t)kPluginFileCap) {
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

        /// @brief Copy a plugin file name with truncation safety.
        /// @param destinationBuffer Destination buffer.
        /// @param sourceText Source name.
        /// @return True when the whole name fit.
        bool CopyFileName(char *destinationBuffer, const char *sourceText)
        {
            if (!destinationBuffer || !sourceText) {
                return false;
            }

            size_t sourceLength = strlen(sourceText);

            if (sourceLength == 0 || sourceLength > (size_t)kPluginFileCap) {
                return false;
            }

            memcpy(destinationBuffer, sourceText, sourceLength + 1);
            return true;
        }

        /// @brief Join the plugin dir and a file name into a path.
        /// @param outputBuffer Output buffer.
        /// @param outputCapacity Buffer capacity.
        /// @param fileName Plugin file name.
        /// @return True when the whole path fit.
        bool JoinPluginPath(char *outputBuffer, unsigned outputCapacity, const char *fileName)
        {
            if (!outputBuffer || outputCapacity == 0 || !fileName) {
                return false;
            }

            size_t dirLength = strlen(s_pluginDir);
            size_t nameLength = strlen(fileName);

            if (nameLength == 0 || nameLength > (size_t)kPluginFileCap) {
                return false;
            }

            if (dirLength + 1 + nameLength + 1 > outputCapacity) {
                return false;
            }

            memcpy(outputBuffer, s_pluginDir, dirLength);
            outputBuffer[dirLength] = '/';
            memcpy(outputBuffer + dirLength + 1, fileName, nameLength + 1);
            return true;
        }

        /// @brief Scan the plugin directory for dnro files, sorted.
        /// @param fileNames Output file names.
        /// @param fileCapacity Table capacity.
        /// @return File count.
        int ScanPluginDir(char fileNames[][kPluginFileCap + 1], int fileCapacity)
        {
            int fileCount = 0;
            DIR *directory = opendir(s_pluginDir);

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
                char sortKey[kPluginFileCap + 1];
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

        /// @brief Whether a plugin id is usable.
        /// @param pluginId Lowercase id.
        /// @return True when valid.
        bool IsPluginIdUsable(const char *pluginId)
        {
            if (!pluginId || !pluginId[0]) {
                return false;
            }

            size_t idLength = strlen(pluginId);

            if (idLength > (size_t)kPluginIdMax) {
                return false;
            }

            for (size_t charIndex = 0; charIndex < idLength; charIndex++) {
                char current = pluginId[charIndex];
                bool lower = current >= 'a' && current <= 'z';
                bool digit = current >= '0' && current <= '9';

                if (!lower && !digit && current != '_') {
                    return false;
                }
            }

            return true;
        }

        /// @brief Upload a button icon PNG to a texture slot.
        /// @param pngBytes PNG bytes (may be null).
        /// @param pngSize PNG byte size.
        /// @return Texture slot or -1.
        int UploadButtonIcon(const unsigned char *pngBytes, unsigned pngSize)
        {
            int textureSlot = -1;

            if (pngBytes && pngSize > 0 && Icons::UploadPng(pngBytes, pngSize, true, &textureSlot)) {
                return textureSlot;
            }

            return -1;
        }

        /// @brief Register one button from a loaded plugin.
        /// @param pluginIndex Loaded plugin index.
        /// @param pluginButton Plugin-local button index.
        /// @return True when the button was registered.
        bool RegisterButton(int pluginIndex, int pluginButton)
        {
            if (s_buttonCount >= kButtonMax) {
                return false;
            }

            PluginInstance *instance = &s_instances[pluginIndex];
            const char *tabId = instance->callbacks.buttonTab ?
                instance->callbacks.buttonTab(pluginButton) : nullptr;

            if (!tabId || !tabId[0] || Tabs::FindById(tabId) < 0) {
                return false;
            }

            ButtonEntry &entry = s_buttons[s_buttonCount];
            entry.pluginIndex = pluginIndex;
            entry.pluginButton = pluginButton;
            entry.iconSlot = -1;
            entry.label[0] = 0;

            if (instance->callbacks.buttonLabel) {
                const char *label = instance->callbacks.buttonLabel(pluginButton);

                if (label && label[0]) {
                    snprintf(entry.label, sizeof(entry.label), "%s", label);
                    entry.label[sizeof(entry.label) - 1] = 0;
                }
            }

            if (instance->callbacks.buttonIcon) {
                unsigned pngSize = 0;
                const unsigned char *pngBytes = instance->callbacks.buttonIcon(pluginButton, &pngSize);
                entry.iconSlot = UploadButtonIcon(pngBytes, pngSize);
            }

            s_buttonCount++;
            return true;
        }

        /// @brief Load one menu plugin dnro and register its buttons.
        /// @param fileName Plugin file name.
        /// @return True on success.
        bool LoadOnePlugin(const char *fileName)
        {
            if (s_instanceCount >= kPluginMax) {
                Log::Line("[menuplugins] table full, skipping %s", fileName);
                return false;
            }

            char filePath[192];

            if (!JoinPluginPath(filePath, sizeof(filePath), fileName)) {
                return false;
            }

            void *handle = dlopen(filePath, RTLD_LOCAL);

            if (!handle) {
                Log::Line("[menuplugins] cannot open %s", fileName);
                return false;
            }

            int (*apiVersion)(void) = (int (*)(void))dlsym(handle, "MenuPluginApiVersion");
            const char *(*pluginId)(void) = (const char *(*)(void))dlsym(handle, "MenuPluginId");
            int (*pluginBoot)(const MenuHostApi *) = (int (*)(const MenuHostApi *))dlsym(handle, "MenuPluginBoot");
            int (*buttonCount)(void) = (int (*)(void))dlsym(handle, "MenuButtonCount");

            if (!apiVersion || !pluginId || !pluginBoot || !buttonCount) {
                Log::Line("[menuplugins] %s misses required exports", fileName);
                dlclose(handle);
                return false;
            }

            if (apiVersion() != kMenuPluginApiVersion) {
                Log::Line("[menuplugins] %s bad api version", fileName);
                dlclose(handle);
                return false;
            }

            const char *idText = pluginId();

            if (!IsPluginIdUsable(idText)) {
                Log::Line("[menuplugins] %s bad id", fileName);
                dlclose(handle);
                return false;
            }

            for (int instanceIndex = 0; instanceIndex < s_instanceCount; instanceIndex++) {
                if (strcmp(s_instances[instanceIndex].pluginId, idText) == 0) {
                    Log::Line("[menuplugins] %s duplicate id", fileName);
                    dlclose(handle);
                    return false;
                }
            }

            if (pluginBoot(s_hostApi) != 0) {
                Log::Line("[menuplugins] %s boot rejected", fileName);
                dlclose(handle);
                return false;
            }

            PluginInstance *instance = &s_instances[s_instanceCount];
            memset(instance, 0, sizeof(*instance));
            CopyFileName(instance->fileName, fileName);
            snprintf(instance->pluginId, sizeof(instance->pluginId), "%s", idText);
            instance->handle = handle;
            instance->callbacks.buttonCount = buttonCount;
            instance->callbacks.buttonTab = (const char *(*)(int))dlsym(handle, "MenuButtonTab");
            instance->callbacks.buttonLabel = (const char *(*)(int))dlsym(handle, "MenuButtonLabel");
            instance->callbacks.buttonIcon =
                (const unsigned char *(*)(int, unsigned *))dlsym(handle, "MenuButtonIconPng");
            instance->callbacks.buttonActivate =
                (int (*)(const MenuHostApi *, int))dlsym(handle, "MenuButtonActivate");
            instance->callbacks.shutdown = (void (*)(void))dlsym(handle, "MenuPluginShutdown");
            instance->buttonBase = s_buttonCount;
            instance->phase = PluginPhase::Ready;

            int registeredCount = buttonCount();

            if (registeredCount < 0) {
                registeredCount = 0;
            }

            int keptButtons = 0;

            for (int buttonIndex = 0; buttonIndex < registeredCount; buttonIndex++) {
                if (RegisterButton(s_instanceCount, buttonIndex)) {
                    keptButtons++;
                }
            }

            instance->buttonCount = keptButtons;
            s_instanceCount++;
            Log::Line("[menuplugins] loaded %s as %s (%d buttons)", fileName, idText, keptButtons);
            return true;
        }

    } // namespace

    /// @brief Boot menu plugins from a pack plugins directory.
    /// @param hostApi Host table with menu functions plus qext.
    /// @param pluginDir Pack plugins dir (for example "sdmc:/qlaunch-ext/NekoMenu/default/plugins").
    void Boot(const MenuHostApi *hostApi, const char *pluginDir)
    {
        if (s_booted) {
            return;
        }

        if (!pluginDir || !pluginDir[0] || strlen(pluginDir) >= (size_t)kPluginDirMax) {
            return;
        }

        s_booted = true;
        s_hostApi = hostApi;
        s_instanceCount = 0;
        s_buttonCount = 0;
        snprintf(s_pluginDir, sizeof(s_pluginDir), "%s", pluginDir);
        s_pluginDir[sizeof(s_pluginDir) - 1] = 0;

        mkdir(s_pluginDir, 0777);

        char fileNames[kPluginScanCap][kPluginFileCap + 1];
        int fileCount = ScanPluginDir(fileNames, kPluginScanCap);
        Log::Line("[menuplugins] found %d files", fileCount);

        for (int fileIndex = 0; fileIndex < fileCount; fileIndex++) {
            LoadOnePlugin(fileNames[fileIndex]);
        }
    }

    /// @brief Shut down menu plugins, freeing icons and unloading dnro files.
    void Shutdown()
    {
        if (!s_booted) {
            return;
        }

        for (int instanceIndex = s_instanceCount - 1; instanceIndex >= 0; instanceIndex--) {
            PluginInstance *instance = &s_instances[instanceIndex];

            if (instance->callbacks.shutdown) {
                instance->callbacks.shutdown();
            }

            if (instance->handle) {
                dlclose(instance->handle);
                instance->handle = nullptr;
            }

            memset(instance, 0, sizeof(*instance));
        }

        for (int buttonIndex = 0; buttonIndex < s_buttonCount; buttonIndex++) {
            if (s_buttons[buttonIndex].iconSlot >= 0) {
                Gfx::TexFree(s_buttons[buttonIndex].iconSlot);
                s_buttons[buttonIndex].iconSlot = -1;
            }
        }

        s_instanceCount = 0;
        s_buttonCount = 0;
        s_hostApi = nullptr;
        s_booted = false;
        s_pluginDir[0] = 0;
    }

    /// @brief Give the total registered button count.
    /// @return Button count across all loaded plugins.
    int ButtonCount()
    {
        return s_buttonCount;
    }

    /// @brief Give a button's tab registry index.
    /// @param button Global button index.
    /// @return Tabs registry index, or -1 when unknown.
    int ButtonTab(int button)
    {
        if (button < 0 || button >= s_buttonCount) {
            return -1;
        }

        PluginInstance *instance = &s_instances[s_buttons[button].pluginIndex];

        if (!instance->callbacks.buttonTab) {
            return -1;
        }

        const char *tabId = instance->callbacks.buttonTab(s_buttons[button].pluginButton);

        if (!tabId || !tabId[0]) {
            return -1;
        }

        return Tabs::FindById(tabId);
    }

    /// @brief Give a button's label.
    /// @param button Global button index.
    /// @return Label, or "" when missing.
    const char *ButtonLabel(int button)
    {
        if (button < 0 || button >= s_buttonCount) {
            return "";
        }

        return s_buttons[button].label;
    }

    /// @brief Give a button's icon texture slot.
    /// @param button Global button index.
    /// @return Texture slot, or -1 when the button has no icon.
    int ButtonIconSlot(int button)
    {
        if (button < 0 || button >= s_buttonCount) {
            return -1;
        }

        return s_buttons[button].iconSlot;
    }

    /// @brief Give the button count for one tab.
    /// @param tabIndex Tabs registry index.
    /// @return Button count targeting that tab.
    int ButtonCountForTab(int tabIndex)
    {
        int tabButtons = 0;

        for (int buttonIndex = 0; buttonIndex < s_buttonCount; buttonIndex++) {
            if (ButtonTab(buttonIndex) == tabIndex) {
                tabButtons++;
            }
        }

        return tabButtons;
    }

    /// @brief Map a tab-local button slot to the global button index.
    /// @param tabIndex Tabs registry index.
    /// @param slot Tab-local slot (0..ButtonCountForTab-1).
    /// @return Global button index, or -1 when out of range.
    int ButtonGlobalAt(int tabIndex, int slot)
    {
        int tabSlot = 0;

        for (int buttonIndex = 0; buttonIndex < s_buttonCount; buttonIndex++) {
            if (ButtonTab(buttonIndex) != tabIndex) {
                continue;
            }

            if (tabSlot == slot) {
                return buttonIndex;
            }

            tabSlot++;
        }

        return -1;
    }

    /// @brief Activate a button through its plugin.
    /// @param button Global button index.
    /// @return Nonzero when the press was consumed.
    int Activate(int button)
    {
        if (button < 0 || button >= s_buttonCount || !s_hostApi) {
            return 0;
        }

        PluginInstance *instance = &s_instances[s_buttons[button].pluginIndex];

        if (instance->phase != PluginPhase::Ready || !instance->callbacks.buttonActivate) {
            return 0;
        }

        return instance->callbacks.buttonActivate(s_hostApi, s_buttons[button].pluginButton) != 0 ? 1 : 0;
    }

} /* namespace MenuPlugins */
