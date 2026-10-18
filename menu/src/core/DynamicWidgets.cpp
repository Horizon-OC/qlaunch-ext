/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */

#include "DynamicWidgets.hpp"
#include "WidgetApi.hpp"
#include "Log.hpp"
#include <dlink/dlink.h>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

namespace DynamicWidgets {

    namespace {

        constexpr char kWidgetBase[] = "sdmc:/qlaunch-ext/NekoMenu";
        constexpr char kWidgetDir[] = "sdmc:/qlaunch-ext/NekoMenu/widgets";
        constexpr int kWidgetMax = 8;
        constexpr int kWidgetFileCap = 63;
        constexpr int kWidgetNameCap = 32;
        constexpr int kWidgetScanCap = 16;

        enum class WidgetPhase {
            Empty,
            Loading,
            Ready,
        };

        struct WidgetCallbacks {
            void (*draw)(const MenuHostApi *hostApi, const WidgetDrawContext *drawContext);
            void (*tick)(const MenuHostApi *hostApi);
            void (*shutdown)(void);
        };

        struct WidgetInstance {
            char fileName[kWidgetFileCap + 1];
            char widgetName[kWidgetNameCap + 1];
            void *handle;
            WidgetCallbacks callbacks;
            WidgetPhase phase;
        };

        static WidgetInstance s_instances[kWidgetMax];
        static int s_instanceCount = 0;
        static const MenuHostApi *s_hostApi = nullptr;
        static bool s_booted = false;

        /// @brief Whether a suffix is dnro.
        /// @param suffix Four-char suffix.
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

            if (nameLength < 6 || nameLength > (size_t)kWidgetFileCap) {
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

        /// @brief Copy a widget file name safely.
        /// @param destinationBuffer Destination buffer.
        /// @param sourceText Source name.
        /// @return True when the name fit.
        bool CopyFileName(char *destinationBuffer, const char *sourceText)
        {
            if (!destinationBuffer || !sourceText) {
                return false;
            }

            size_t sourceLength = strlen(sourceText);

            if (sourceLength == 0 || sourceLength > (size_t)kWidgetFileCap) {
                return false;
            }

            memcpy(destinationBuffer, sourceText, sourceLength + 1);
            return true;
        }

        /// @brief Join the widget dir and file name.
        /// @param outputBuffer Output buffer.
        /// @param outputCapacity Buffer capacity.
        /// @param fileName Widget file name.
        /// @return True when the path fit.
        bool JoinWidgetPath(char *outputBuffer, unsigned outputCapacity, const char *fileName)
        {
            if (!outputBuffer || outputCapacity == 0 || !fileName) {
                return false;
            }

            size_t dirLength = strlen(kWidgetDir);
            size_t nameLength = strlen(fileName);

            if (nameLength == 0 || nameLength > (size_t)kWidgetFileCap) {
                return false;
            }

            if (dirLength + 1 + nameLength + 1 > outputCapacity) {
                return false;
            }

            memcpy(outputBuffer, kWidgetDir, dirLength);
            outputBuffer[dirLength] = '/';
            memcpy(outputBuffer + dirLength + 1, fileName, nameLength + 1);
            return true;
        }

        /// @brief Scan the widget directory, sorted.
        /// @param fileNames Output names.
        /// @param fileCapacity Table capacity.
        /// @return File count.
        int ScanWidgetDir(char fileNames[][kWidgetFileCap + 1], int fileCapacity)
        {
            int fileCount = 0;
            DIR *directory = opendir(kWidgetDir);

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
                char sortKey[kWidgetFileCap + 1];
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

        /// @brief Whether a widget name is usable.
        /// @param widgetName Lowercase name.
        /// @return True when valid.
        bool IsWidgetNameUsable(const char *widgetName)
        {
            if (!widgetName || !widgetName[0]) {
                return false;
            }

            size_t nameLength = strlen(widgetName);

            if (nameLength >= (size_t)kWidgetNameCap) {
                return false;
            }

            for (size_t charIndex = 0; charIndex < nameLength; charIndex++) {
                char current = widgetName[charIndex];
                bool lower = current >= 'a' && current <= 'z';
                bool digit = current >= '0' && current <= '9';

                if (!lower && !digit && current != '_') {
                    return false;
                }
            }

            return true;
        }

        /// @brief Load one widget dnro.
        /// @param fileName Widget file name.
        /// @return True on success.
        bool LoadOneWidget(const char *fileName)
        {
            if (s_instanceCount >= kWidgetMax) {
                Log::Line("[widgets] table full, skipping %s", fileName);
                return false;
            }

            char filePath[160];

            if (!JoinWidgetPath(filePath, sizeof(filePath), fileName)) {
                return false;
            }

            void *handle = dlopen(filePath, RTLD_LOCAL);

            if (!handle) {
                Log::Line("[widgets] cannot open %s", fileName);
                return false;
            }

            int (*apiVersion)(void) = (int (*)(void))dlsym(handle, "WidgetApiVersion");
            const char *(*widgetName)(void) = (const char *(*)(void))dlsym(handle, "WidgetName");
            int (*widgetBoot)(const MenuHostApi *) = (int (*)(const MenuHostApi *))dlsym(handle, "WidgetBoot");
            void (*widgetDraw)(const MenuHostApi *, const WidgetDrawContext *) = (void (*)(const MenuHostApi *, const WidgetDrawContext *))dlsym(handle, "WidgetDraw");

            if (!apiVersion || !widgetName || !widgetBoot || !widgetDraw) {
                Log::Line("[widgets] %s misses required exports", fileName);
                dlclose(handle);
                return false;
            }

            if (apiVersion() != kWidgetApiVersion) {
                Log::Line("[widgets] %s bad api version", fileName);
                dlclose(handle);
                return false;
            }

            const char *instanceName = widgetName();

            if (!IsWidgetNameUsable(instanceName)) {
                Log::Line("[widgets] %s bad name", fileName);
                dlclose(handle);
                return false;
            }

            for (int instanceIndex = 0; instanceIndex < s_instanceCount; instanceIndex++) {
                if (strcmp(s_instances[instanceIndex].widgetName, instanceName) == 0) {
                    Log::Line("[widgets] %s duplicate name", fileName);
                    dlclose(handle);
                    return false;
                }
            }

            if (widgetBoot(s_hostApi) != 0) {
                Log::Line("[widgets] %s boot rejected", fileName);
                dlclose(handle);
                return false;
            }

            WidgetInstance *instance = &s_instances[s_instanceCount++];
            memset(instance, 0, sizeof(*instance));
            CopyFileName(instance->fileName, fileName);
            snprintf(instance->widgetName, sizeof(instance->widgetName), "%s", instanceName);
            instance->handle = handle;
            instance->callbacks.draw = widgetDraw;
            instance->callbacks.tick = (void (*)(const MenuHostApi *))dlsym(handle, "WidgetTick");
            instance->callbacks.shutdown = (void (*)(void))dlsym(handle, "WidgetShutdown");
            instance->phase = WidgetPhase::Ready;
            Log::Line("[widgets] loaded %s as %s", fileName, instanceName);
            return true;
        }

    } // namespace

    /// @brief Boot dynamic widgets from the NekoMenu directory.
    /// @param hostApi Host table with menu functions plus qext.
    void Boot(const MenuHostApi *hostApi)
    {
        if (s_booted) {
            return;
        }

        s_booted = true;
        s_hostApi = hostApi;
        s_instanceCount = 0;

        mkdir(kWidgetBase, 0777);
        mkdir(kWidgetDir, 0777);

        char fileNames[kWidgetScanCap][kWidgetFileCap + 1];
        int fileCount = ScanWidgetDir(fileNames, kWidgetScanCap);
        Log::Line("[widgets] found %d files", fileCount);

        for (int fileIndex = 0; fileIndex < fileCount; fileIndex++) {
            LoadOneWidget(fileNames[fileIndex]);
        }
    }

    /// @brief Shut down widgets and unload dnro files.
    void Shutdown()
    {
        if (!s_booted) {
            return;
        }

        for (int instanceIndex = s_instanceCount - 1; instanceIndex >= 0; instanceIndex--) {
            WidgetInstance *instance = &s_instances[instanceIndex];

            if (instance->callbacks.shutdown) {
                instance->callbacks.shutdown();
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
    }

    /// @brief Tick loaded widgets once per frame.
    void Tick()
    {
        if (!s_booted || !s_hostApi) {
            return;
        }

        for (int instanceIndex = 0; instanceIndex < s_instanceCount; instanceIndex++) {
            if (s_instances[instanceIndex].phase != WidgetPhase::Ready) {
                continue;
            }

            if (s_instances[instanceIndex].callbacks.tick) {
                s_instances[instanceIndex].callbacks.tick(s_hostApi);
            }
        }
    }

    /// @brief Whether a named widget is loaded.
    /// @param widgetName Widget name.
    /// @return True when loaded.
    bool HasWidget(const char *widgetName)
    {
        if (!widgetName) {
            return false;
        }

        for (int instanceIndex = 0; instanceIndex < s_instanceCount; instanceIndex++) {
            if (strcmp(s_instances[instanceIndex].widgetName, widgetName) == 0) {
                return s_instances[instanceIndex].phase == WidgetPhase::Ready;
            }
        }

        return false;
    }

    /// @brief Draw all widgets onto the core state.
    void DrawAll()
    {
        if (!s_booted || !s_hostApi) {
            return;
        }

        WidgetDrawContext drawContext;
        drawContext.slotX = 1600.0f;
        drawContext.slotY = 60.0f;
        drawContext.slotWidth = 260.0f;
        drawContext.slotHeight = 100.0f;

        for (int instanceIndex = 0; instanceIndex < s_instanceCount; instanceIndex++) {
            if (s_instances[instanceIndex].phase != WidgetPhase::Ready) {
                continue;
            }

            if (s_instances[instanceIndex].callbacks.draw) {
                s_instances[instanceIndex].callbacks.draw(s_hostApi, &drawContext);
            }
        }
    }

} /* namespace DynamicWidgets */
