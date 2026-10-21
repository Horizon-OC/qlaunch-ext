#pragma once
#include "Layout.hpp"

#ifdef __cplusplus
extern "C" {
#endif

    constexpr int kAppNameMax = 192;
    constexpr int kPopupMaxItems = 12;
    constexpr int kPopupTextMax = 192;

    constexpr int kPopupLayoutList = 0;
    constexpr int kPopupLayoutRow = 1;

    constexpr int kPopupResultNone = 0;
    constexpr int kPopupResultConfirmed = 1;
    constexpr int kPopupResultCancelled = 2;

    /// @brief One row in the home game strip (owned by menu.dnro).
    struct AppRow {
        bool isGame;
        bool isFolder;
        u64 tid;
        u64 folderId;
        int folderColor;
        int applet;
        int iconSlot;
        bool ejected;
        char name[kAppNameMax];
    };

    constexpr int kMenuHostApiVersion = 1;

    struct WidgetDrawContext {
        float slotX;
        float slotY;
        float slotWidth;
        float slotHeight;
    };

    struct MenuHostApi {
        int version;
        void (*gfxPushPanel)(float positionX, float positionY, float width, float height, float cornerRadius,
                             float red, float green, float blue, float alpha);
        unsigned (*gfxPushIcon)(float positionX, float positionY, float width, float height,
                                float cornerTopLeft, float cornerTopRight, float cornerBottomRight, float cornerBottomLeft,
                                float red, float green, float blue, float border, int textureSlot,
                                float leftU, float topV, float rightU, float bottomV, float edgeBlend, float alpha);
        void (*gfxPushQuad)(float positionX, float positionY, float width, float height,
                            float red, float green, float blue, float alpha);
        void (*gfxPushSelectRing)(float positionX, float positionY, float width, float height,
                                  float cornerRadius, float thickness);
        int (*gfxTexAlloc)(void);
        int (*gfxTexValid)(int textureSlot);
        void (*gfxTexFree)(int textureSlot);
        int (*gfxTexUploadRgba)(int textureSlot, const unsigned char *pixelData, int imageWidth, int imageHeight);
        float (*fontDraw)(const char *text, float positionX, float baselineY, float pixelSize,
                          float red, float green, float blue, float alpha);
        float (*fontMeasure)(const char *text, float pixelSize);
        void (*fontCentered)(const char *text, float centerX, float baselineY, float pixelSize, float maxWidth,
                             float red, float green, float blue, float alpha);
        void (*themeColor)(const char *key, float *redOut, float *greenOut, float *blueOut);
        void (*themeMenuColor)(int menuIndex, float *redOut, float *greenOut, float *blueOut);
        void (*themeMenuAccent)(int menuIndex, float *redOut, float *greenOut, float *blueOut);
        int (*themeIsDark)(void);
        int (*iconsHudSlot)(int hudId);
        float (*layoutShiftX)(void);
        int (*appMenu)(void);
        float (*appTime)(void);
        void (*switchMenuById)(const char *tabId);
        const char *(*clockTimeStr)(void);
        unsigned (*clockBatt)(void);
        int (*clockCharging)(void);
        int (*clockWifi)(void);
        int (*clockNetType)(void);
        int (*clockPlane)(void);
        void (*sfxPlayByName)(const char *clipName);
        void (*logText)(const char *text);
        int (*topFocus)(void);
        void (*setTopFocus)(int focused);
        void (*setTopSelection)(int tabIndex);
        int (*themeCount)(void);
        const char *(*themeName)(int themeIndex);
        int (*setTheme)(int themeIndex);
        int (*getTheme)(void);
        void (*themePreview)(int themeIndex, float *redOut, float *greenOut, float *blueOut);
        int (*themeFileData)(const unsigned char **outputData, unsigned *outputSize);
        void (*themeTabColor)(const char *tabId, float *redOut, float *greenOut, float *blueOut);
        void (*themeTabAccent)(const char *tabId, float *redOut, float *greenOut, float *blueOut);
        int (*qextRefreshTitles)(void);
        int (*qextTitleCount)(void);
        unsigned long long (*qextTitleId)(int titleIndex);
        int (*qextTitleName)(int titleIndex, char *outputBuffer, unsigned outputCapacity);
        int (*qextTitleIconSize)(int titleIndex);
        int (*qextTitleIcon)(int titleIndex, void *outputBuffer, unsigned bufferCapacity);
        int (*qextTitleEjected)(int titleIndex);
        int (*qextAlbumRefresh)(void);
        int (*qextAlbumCount)(void);
        int (*qextAlbumThumb)(int albumIndex, void *outputBuffer, unsigned bufferCapacity);
        int (*qextAlbumImageSize)(int albumIndex);
        int (*qextAlbumImage)(int albumIndex, void *outputBuffer, unsigned bufferCapacity);
        int (*qextAlbumIsMovie)(int albumIndex);
        int (*qextLaunchTitle)(unsigned long long titleId);
        int (*qextSleep)(void);
        int (*qextLaunchApplet)(int appletKind);
        int (*qextGameRunning)(void);
        int (*qextGameHasForeground)(void);
        unsigned long long (*qextSuspendedTitle)(void);
        int (*qextBatteryPercent)(void);
        int (*qextWifiSignal)(void);
        int (*qextConnectionType)(void);
        int (*qextAirplaneMode)(void);
        int (*gfxTexLock)(int textureSlot, int width, int height, void **pixelsOut, int *pitchOut);
        void (*gfxTexUnlock)(int textureSlot);
        void (*sfxVideoBegin)(void);
        void (*sfxVideoEnd)(void);
        void (*sfxVideoPush)(const short *pcmFrames, unsigned frameCount);
        void (*sfxVideoSeekTo)(double positionSeconds);
        double (*sfxVideoBuffered)(void);
        void (*sfxVideoSetPaused)(int paused);
        int (*qextAlbumMovieOpen)(int albumIndex);
        unsigned long long (*qextAlbumMovieSize)(void);
        int (*qextAlbumMovieRead)(unsigned long long offset, void *outputBuffer, unsigned bufferCapacity);
        void (*qextAlbumMovieClose)(void);
        int (*qextAlbumGrow)(void);
        void (*gfxPushOverlayPanel)(float,float,float,float,float,float,float,float,float);
        /* Layout + Home module surface (append-only additions). */
        int (*topSelection)(void);
        int (*bottomFocus)(void);
        void (*setBottomFocus)(int focused);
        int (*bottomSelection)(void);
        void (*setBottomSelection)(int selection);
        int (*menuFrom)(void);
        float (*menuT)(void);
        int (*menuDir)(void);
        int (*menuPhase)(void);
        void (*menuJingle)(int tabIndex);
        int (*homeSub)(void);
        void (*setHomeSub)(int sub);
        float (*homeShiftY)(void);
        int (*homeShiftBusy)(void);
        int (*rowCount)(void);
        const AppRow* (*rowAt)(int rowIndex);
        int (*appSelection)(void);
        void (*appSetSelection)(int selection);
        int (*tabFindById)(const char *tabId);
        int (*tabCount)(void);
        int (*tabGlyph)(int tabIndex);
        int (*tabHudSelect)(int tabIndex);
        int (*tabHudDeselect)(int tabIndex);
        const char* (*tabIdAt)(int tabIndex);
        const char* (*clockAmPm)(void);
        int (*avatarCount)(void);
        int (*avatarSlot)(int avatarIndex);
        unsigned (*padGlyph)(void);
        void (*padIconColor)(float*, float*, float*);
        int (*submenuIsDynamicTab)(int tabIndex);
        int (*submenuTabColor)(int tabIndex, float*, float*, float*);
        int (*widgetHas)(const char *widgetName);
        void (*widgetDrawAll)(void);
        void (*iconsSyncTitle)(const u64*, int*, int count, int focus);
        unsigned (*fontNext)(const char**);
        int (*themeTryColor)(const char*, float*, float*, float*);
        void (*popupOpen)(int layout, const char*, const char*, const char* const*, const float*, int, int, unsigned long long);
        void (*popupClose)(void);
        int (*popupIsOpen)(void);
        int (*popupSelection)(void);
        int (*popupInput)(unsigned long long, unsigned long long);
        void (*popupSetGridColumns)(int columns);
        void (*refreshTitles)(void);
        int (*launchGameRow)(unsigned long long, int ejected, unsigned long long);
        void (*gfxPushLayer)(int layer);
        void (*gfxPopLayer)(void);
        void (*gfxPushPanelCorners)(float,float,float,float, float,float,float,float, float,float,float,float);
        void (*gfxPushOverlayQuad)(float,float,float,float, float,float,float,float);
        void (*gfxPushOverlayRing)(float,float,float,float,float,float);
        int (*submenuDrawTab)(int tabIndex);
        int (*submenuInputTab)(int tabIndex, unsigned long long buttonsDown, unsigned long long buttonsHeld);
        int (*submenuFullTabIndex)(void);
        int (*submenuTabBackgroundColor)(int tabIndex, float*, float*, float*);
        int (*tabSetJingle)(int tabIndex, const char *jingle);
        int (*layoutFindNode)(const char *type, struct LayoutNode *nodeOut);
        int (*appTopSwitch)(int delta);
        int (*appActivateBottom)(void);
        int (*appActivate)(unsigned long long heldButtons);
        int (*promptOpenGame)(unsigned long long heldButtons, unsigned long long launchTid);
        void (*openFolderView)(unsigned long long folderId);
        void (*closeFolderView)(void);
        unsigned long long (*openFolder)(void);
        int (*folderSel)(void);
        void (*setFolderSel)(int selection);
        void (*selectFolderRow)(unsigned long long folderId);
        int (*layoutTabCount)(void);
        int (*layoutTabAt)(int slot);
        float (*menuSlideX)(void);
        int (*touchRegisterCell)(int index, float x, float y, float w, float h);
        int (*touchHitTest)(int tabIndex, float x, float y, int *indexOut);
        int (*submenuDrag)(int tabIndex, float moveX, float moveY, int phase);
        int (*submenuTouchAction)(int tabIndex, int cellIndex);
        int (*submenuTap)(int tabIndex, float x, float y);
        int (*popupCellAt)(float x, float y);
        void (*popupSelectCell)(int index);
        int (*takeTap)(float *xOut, float *yOut);
        int (*submenuTouchDown)(int tabIndex, float x, float y);
        int (*submenuTouchUp)(int tabIndex, float x, float y);
        int (*packAssetSize)(const char *assetName);
        int (*packAssetRead)(const char *assetName, void *outputBuffer, unsigned bufferCapacity);
    };

    /// @brief Fill background, ink, and accent colors from the theme configuration.
    /// @param hostApi Host API.
    /// @param tabId Tab id (for example "settings").
    /// @param backgroundOut Background output (3 floats).
    /// @param inkOut Ink output (3 floats).
    /// @param accentOut Accent output (3 floats).
    static inline void MenuHostThemeColors(const struct MenuHostApi *hostApi, const char *tabId,
                                           float backgroundOut[3], float inkOut[3], float accentOut[3])
    {
        if (!hostApi || !tabId || !backgroundOut || !inkOut || !accentOut) {
            return;
        }

        hostApi->themeTabColor(tabId, &backgroundOut[0], &backgroundOut[1], &backgroundOut[2]);
        hostApi->themeColor("ink", &inkOut[0], &inkOut[1], &inkOut[2]);
        hostApi->themeTabAccent(tabId, &accentOut[0], &accentOut[1], &accentOut[2]);
    }

#ifdef __cplusplus
}
#endif
