/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */

#include "HostApi.hpp"
#include "Gfx.hpp"
#include "Font.hpp"
#include "Theme.hpp"
#include "Icons.hpp"
#include "Layout.hpp"
#include "App.hpp"
#include "Clock.hpp"
#include "Sfx.hpp"
#include "Qext.hpp"
#include "Log.hpp"
#include "Tabs.hpp"
#include "Avatars.hpp"
#include "PadIcon.hpp"
#include "Submenus.hpp"
#include "DynamicWidgets.hpp"
#include "LayoutMod.hpp"
#include "Touch.hpp"
#include "Packs.hpp"
#include "../widgets/Widgets.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace HostApi {

    namespace {

        void GfxPushPanelThunk(float positionX, float positionY, float width, float height, float cornerRadius,
                               float red, float green, float blue, float alpha)
        {
            Gfx::PushPanel(positionX, positionY, width, height, cornerRadius, red, green, blue, alpha);
        }

        unsigned GfxPushIconThunk(float positionX, float positionY, float width, float height,
                                  float cornerTopLeft, float cornerTopRight, float cornerBottomRight, float cornerBottomLeft,
                                  float red, float green, float blue, float border, int textureSlot,
                                  float leftU, float topV, float rightU, float bottomV, float edgeBlend, float alpha)
        {
            return Gfx::PushIcon(positionX, positionY, width, height,
                                 cornerTopLeft, cornerTopRight, cornerBottomRight, cornerBottomLeft,
                                 red, green, blue, border, textureSlot,
                                 leftU, topV, rightU, bottomV, edgeBlend, alpha);
        }

        void GfxPushQuadThunk(float positionX, float positionY, float width, float height,
                              float red, float green, float blue, float alpha)
        {
            Gfx::PushQuad(positionX, positionY, width, height, red, green, blue, alpha);
        }

        void GfxPushSelectRingThunk(float positionX, float positionY, float width, float height,
                                    float cornerRadius, float thickness)
        {
            Gfx::PushSelectRing(positionX, positionY, width, height, cornerRadius, thickness);
        }

        int GfxTexAllocThunk(void)
        {
            return Gfx::TexAlloc();
        }

        int GfxTexValidThunk(int textureSlot)
        {
            return Gfx::TexValid(textureSlot) ? 1 : 0;
        }

        void GfxTexFreeThunk(int textureSlot)
        {
            Gfx::TexFree(textureSlot);
        }

        int GfxTexUploadRgbaThunk(int textureSlot, const unsigned char *pixelData, int imageWidth, int imageHeight)
        {
            return Gfx::TexUpload(textureSlot, pixelData, imageWidth, imageHeight, Gfx::TexFormat::RGBA) ? 1 : 0;
        }

        float FontDrawThunk(const char *text, float positionX, float baselineY, float pixelSize,
                            float red, float green, float blue, float alpha)
        {
            return Font::Draw(text, positionX, baselineY, pixelSize, red, green, blue, alpha);
        }

        float FontMeasureThunk(const char *text, float pixelSize)
        {
            return Font::Measure(text, pixelSize);
        }

        void FontCenteredThunk(const char *text, float centerX, float baselineY, float pixelSize, float maxWidth,
                               float red, float green, float blue, float alpha)
        {
            Font::Centered(text, centerX, baselineY, pixelSize, maxWidth, red, green, blue, alpha);
        }

        void ThemeColorThunk(const char *key, float *redOut, float *greenOut, float *blueOut)
        {
            Theme::Color(key, redOut, greenOut, blueOut);
        }

        void ThemeMenuColorThunk(int menuIndex, float *redOut, float *greenOut, float *blueOut)
        {
            Theme::MenuColor(menuIndex, redOut, greenOut, blueOut);
        }

        void ThemeMenuAccentThunk(int menuIndex, float *redOut, float *greenOut, float *blueOut)
        {
            Theme::MenuAccent(menuIndex, redOut, greenOut, blueOut);
        }

        int ThemeIsDarkThunk(void)
        {
            return Theme::IsDark() ? 1 : 0;
        }

        int IconsHudSlotThunk(int hudId)
        {
            return Icons::HudSlot(hudId);
        }

        float LayoutShiftXThunk(void)
        {
            return LayoutMod::ShiftX();
        }

        int AppMenuThunk(void)
        {
            return App::Menu();
        }

        float AppTimeThunk(void)
        {
            return App::Time();
        }

        void SwitchMenuByIdThunk(const char *tabId)
        {
            App::SwitchMenuById(tabId);
        }

        const char *ClockTimeStrThunk(void)
        {
            return Clock::TimeStr();
        }

        unsigned ClockBattThunk(void)
        {
            return Clock::Batt();
        }

        int ClockChargingThunk(void)
        {
            return Clock::Charging();
        }

        int ClockWifiThunk(void)
        {
            return Clock::Wifi();
        }

        int ClockNetTypeThunk(void)
        {
            return Clock::NetType();
        }

        int ClockPlaneThunk(void)
        {
            return Clock::Plane();
        }

        void SfxPlayByNameThunk(const char *clipName)
        {
            Sfx::PlayByName(clipName);
        }

        int TopFocusThunk(void)
        {
            return App::TopFocus() ? 1 : 0;
        }

        void SetTopFocusThunk(int focused)
        {
            App::SetTopFocus(focused != 0);
        }

        void SetTopSelectionThunk(int tabIndex)
        {
            App::SetTopSel(tabIndex);
        }

        int ThemeCountThunk(void)
        {
            return Theme::ThemeCount();
        }

        const char *ThemeNameThunk(int themeIndex)
        {
            return Theme::ThemeName(themeIndex);
        }

        int SetThemeThunk(int themeIndex)
        {
            return Theme::SetTheme(themeIndex) ? 1 : 0;
        }

        int GetThemeThunk(void)
        {
            return Theme::GetTheme();
        }

        void ThemePreviewThunk(int themeIndex, float *redOut, float *greenOut, float *blueOut)
        {
            Theme::ThemePreview(themeIndex, redOut, greenOut, blueOut);
        }

        int ThemeFileDataThunk(const unsigned char **outputData, unsigned *outputSize)
        {
            return Theme::ThemeFile(outputData, outputSize) ? 1 : 0;
        }

        void ThemeTabColorThunk(const char *tabId, float *redOut, float *greenOut, float *blueOut)
        {
            Theme::MenuColor(Theme::MenuSlotForId(tabId), redOut, greenOut, blueOut);
        }

        void ThemeTabAccentThunk(const char *tabId, float *redOut, float *greenOut, float *blueOut)
        {
            Theme::MenuAccent(Theme::MenuSlotForId(tabId), redOut, greenOut, blueOut);
        }

        void LogTextThunk(const char *text)
        {
            if (!text) {
                return;
            }

            Log::Line("%s", text);
        }

        int QextRefreshTitlesThunk(void)
        {
            return qext_refresh_titles();
        }

        int QextTitleCountThunk(void)
        {
            return qext_title_count();
        }

        unsigned long long QextTitleIdThunk(int titleIndex)
        {
            return qext_title_id(titleIndex);
        }

        int QextTitleNameThunk(int titleIndex, char *outputBuffer, unsigned outputCapacity)
        {
            return qext_title_name(titleIndex, outputBuffer, outputCapacity);
        }

        int QextTitleIconSizeThunk(int titleIndex)
        {
            return qext_title_icon_size(titleIndex);
        }

        int QextTitleIconThunk(int titleIndex, void *outputBuffer, unsigned bufferCapacity)
        {
            return qext_title_icon(titleIndex, outputBuffer, bufferCapacity);
        }

        int QextTitleEjectedThunk(int titleIndex)
        {
            return qext_title_ejected(titleIndex);
        }

        int QextAlbumRefreshThunk(void)
        {
            return qext_album_refresh();
        }

        int QextAlbumCountThunk(void)
        {
            return qext_album_count();
        }

        int QextAlbumThumbThunk(int albumIndex, void *outputBuffer, unsigned bufferCapacity)
        {
            return qext_album_thumb(albumIndex, outputBuffer, bufferCapacity);
        }

        int QextAlbumImageSizeThunk(int albumIndex)
        {
            return qext_album_image_size(albumIndex);
        }

        int QextAlbumImageThunk(int albumIndex, void *outputBuffer, unsigned bufferCapacity)
        {
            return qext_album_image(albumIndex, outputBuffer, bufferCapacity);
        }

        int QextAlbumIsMovieThunk(int albumIndex)
        {
            return qext_album_is_movie(albumIndex);
        }

        int QextLaunchTitleThunk(unsigned long long titleId)
        {
            return (int)qext_launch_title((u64)titleId);
        }

        int QextSleepThunk(void)
        {
            return (int)qext_sleep();
        }

        int QextLaunchAppletThunk(int appletKind)
        {
            return (int)qext_launch_applet(appletKind);
        }

        int QextGameRunningThunk(void)
        {
            return qext_game_running();
        }

        int QextGameHasForegroundThunk(void)
        {
            return qext_game_has_foreground();
        }

        unsigned long long QextSuspendedTitleThunk(void)
        {
            return qext_suspended_title();
        }

        int QextBatteryPercentThunk(void)
        {
            return qext_battery_percent();
        }

        int QextWifiSignalThunk(void)
        {
            return qext_wifi_signal();
        }

        int QextConnectionTypeThunk(void)
        {
            return qext_connection_type();
        }

        int QextAirplaneModeThunk(void)
        {
            return qext_airplane_mode();
        }

        int GfxTexLockThunk(int textureSlot, int width, int height, void **pixelsOut, int *pitchOut)
        {
            return Gfx::TexLock(textureSlot, width, height, pixelsOut, pitchOut) ? 1 : 0;
        }

        void GfxTexUnlockThunk(int textureSlot)
        {
            Gfx::TexUnlock(textureSlot);
        }

        void SfxVideoBeginThunk(void)
        {
            Sfx::VideoBegin();
        }

        void SfxVideoEndThunk(void)
        {
            Sfx::VideoEnd();
        }

        void SfxVideoPushThunk(const short *pcmFrames, unsigned frameCount)
        {
            Sfx::VideoPush(pcmFrames, frameCount);
        }

        void SfxVideoSeekToThunk(double positionSeconds)
        {
            Sfx::VideoSeekTo(positionSeconds);
        }

        double SfxVideoBufferedThunk(void)
        {
            return Sfx::VideoBuffered();
        }

        void SfxVideoSetPausedThunk(int paused)
        {
            Sfx::VideoSetPaused(paused != 0);
        }

        int QextAlbumMovieOpenThunk(int albumIndex)
        {
            return qext_album_movie_open(albumIndex) ? 1 : 0;
        }

        unsigned long long QextAlbumMovieSizeThunk(void)
        {
            return qext_album_movie_size();
        }

        int QextAlbumMovieReadThunk(unsigned long long offset, void *outputBuffer, unsigned bufferCapacity)
        {
            return qext_album_movie_read((u64)offset, outputBuffer, bufferCapacity);
        }

        void QextAlbumMovieCloseThunk(void)
        {
            qext_album_movie_close();
        }

        int QextAlbumGrowThunk(void)
        {
            return qext_album_grow();
        }

        void GfxPushOverlayPanelThunk(float positionX, float positionY, float width, float height, float cornerRadius,
                                      float red, float green, float blue, float alpha)
        {
            Gfx::PushOverlayPanel(positionX, positionY, width, height, cornerRadius, red, green, blue, alpha);
        }

        int TopSelectionThunk(void)
        {
            return App::TopSel();
        }

        int BottomFocusThunk(void)
        {
            return App::BottomFocus() ? 1 : 0;
        }

        void SetBottomFocusThunk(int focused)
        {
            App::SetBottomFocus(focused != 0);
        }

        int BottomSelectionThunk(void)
        {
            return App::BottomSel();
        }

        void SetBottomSelectionThunk(int selection)
        {
            App::SetBottomSel(selection);
        }

        int MenuFromThunk(void)
        {
            return App::MenuFrom();
        }

        float MenuTThunk(void)
        {
            return App::MenuT();
        }

        int MenuDirThunk(void)
        {
            return App::MenuDir();
        }

        int MenuPhaseThunk(void)
        {
            return (int)App::MenuPhaseOf();
        }

        float MenuSlideXThunk(void)
        {
            if (App::MenuPhaseOf() != MenuPhase_Animating || App::MenuT() >= 1.0f) {
                return 0.0f;
            }

            float progress = App::MenuT();

            if (progress < 0.0f) {
                progress = 0.0f;
            }

            if (progress > 1.0f) {
                progress = 1.0f;
            }

            float eased = 1.0f - (1.0f - progress) * (1.0f - progress) * (1.0f - progress);
            return -eased * (float)App::MenuDir() * 1920.0f;
        }

        void MenuJingleThunk(int tabIndex)
        {
            App::MenuJingle(tabIndex);
        }

        int HomeSubThunk(void)
        {
            return App::HomeSub();
        }

        void SetHomeSubThunk(int sub)
        {
            App::SetHomeSub(sub);
        }

        float HomeShiftYThunk(void)
        {
            return App::HomeShiftY();
        }

        int HomeShiftBusyThunk(void)
        {
            return App::HomeShiftBusy() ? 1 : 0;
        }

        int RowCountThunk(void)
        {
            return App::RowCount();
        }

        const AppRow *RowAtThunk(int rowIndex)
        {
            return &App::Row(rowIndex);
        }

        int AppSelectionThunk(void)
        {
            return App::Sel();
        }

        void AppSetSelectionThunk(int selection)
        {
            App::SetSel(selection);
        }

        int AppTopSwitchThunk(int delta)
        {
            App::TopSwitch(delta);
            return 1;
        }

        int AppActivateBottomThunk(void)
        {
            App::ActivateBottom();
            return 1;
        }

        int AppActivateThunk(unsigned long long heldButtons)
        {
            return App::Activate(heldButtons) ? 1 : 0;
        }

        int PromptOpenGameThunk(unsigned long long heldButtons, unsigned long long launchTid)
        {
            WPrompt::OpenCloseGame(heldButtons, launchTid);
            return 1;
        }

        void OpenFolderViewThunk(unsigned long long folderId)
        {
            App::OpenFolderView(folderId);
        }

        void CloseFolderViewThunk(void)
        {
            App::CloseFolderView();
        }

        unsigned long long OpenFolderThunk(void)
        {
            return App::OpenFolder();
        }

        int FolderSelThunk(void)
        {
            return App::FolderSel();
        }

        void SetFolderSelThunk(int selection)
        {
            App::SetFolderSel(selection);
        }

        void SelectFolderRowThunk(unsigned long long folderId)
        {
            App::SelectFolderRow(folderId);
        }

        int TabFindByIdThunk(const char *tabId)
        {
            return Tabs::FindById(tabId);
        }

        int TabCountThunk(void)
        {
            return Tabs::Count();
        }

        int TabGlyphThunk(int tabIndex)
        {
            const MenuTab *tab = Tabs::Get(tabIndex);
            return tab ? (int)tab->glyph : 0;
        }

        int TabHudSelectThunk(int tabIndex)
        {
            const MenuTab *tab = Tabs::Get(tabIndex);
            return tab ? tab->hudSel : -1;
        }

        int TabHudDeselectThunk(int tabIndex)
        {
            const MenuTab *tab = Tabs::Get(tabIndex);
            return tab ? tab->hudDesel : -1;
        }

        const char *TabIdAtThunk(int tabIndex)
        {
            const MenuTab *tab = Tabs::Get(tabIndex);
            return tab ? tab->id : "";
        }

        const char *ClockAmPmThunk(void)
        {
            return Clock::AmPm();
        }

        int AvatarCountThunk(void)
        {
            return Avatars::Count();
        }

        int AvatarSlotThunk(int avatarIndex)
        {
            return Avatars::Slot(avatarIndex);
        }

        unsigned PadGlyphThunk(void)
        {
            return PadIcon::Glyph();
        }

        void PadIconColorThunk(float *redOut, float *greenOut, float *blueOut)
        {
            PadIcon::Color(redOut, greenOut, blueOut);
        }

        int SubmenuIsDynamicTabThunk(int tabIndex)
        {
            return Submenus::IsDynamicTab(tabIndex) ? 1 : 0;
        }

        int SubmenuTabColorThunk(int tabIndex, float *redOut, float *greenOut, float *blueOut)
        {
            return Submenus::TabColor(tabIndex, redOut, greenOut, blueOut) ? 1 : 0;
        }

        int WidgetHasThunk(const char *widgetName)
        {
            return DynamicWidgets::HasWidget(widgetName) ? 1 : 0;
        }

        void WidgetDrawAllThunk(void)
        {
            DynamicWidgets::DrawAll();
        }

        void IconsSyncTitleThunk(const u64 *titleIds, int *outputSlots, int rowCount, int focus)
        {
            Icons::Sync(titleIds, outputSlots, rowCount, focus);
        }

        unsigned FontNextThunk(const char **textCursor)
        {
            return Font::Next(textCursor);
        }

        int ThemeTryColorThunk(const char *key, float *redOut, float *greenOut, float *blueOut)
        {
            return Theme::TryColor(key, redOut, greenOut, blueOut) ? 1 : 0;
        }

        void PopupOpenThunk(int layout, const char *titleText, const char *messageText, const char *const *labels,
                            const float *swatches, int labelCount, int initialSelection, unsigned long long heldButtons)
        {
            WPopup::Open((WPopup::PopupLayout)layout, titleText, messageText, labels, swatches, labelCount,
                         initialSelection, heldButtons);
        }

        void PopupCloseThunk(void)
        {
            WPopup::Close();
        }

        int PopupIsOpenThunk(void)
        {
            return WPopup::IsOpen() ? 1 : 0;
        }

        int PopupSelectionThunk(void)
        {
            return WPopup::Selection();
        }

        int PopupInputThunk(unsigned long long buttonsDown, unsigned long long buttonsHeld)
        {
            return (int)WPopup::Input(buttonsDown, buttonsHeld);
        }

        void PopupSetGridColumnsThunk(int columns)
        {
            WPopup::SetGridColumns(columns);
        }

        void RefreshTitlesThunk(void)
        {
            App::RefreshTitles();
        }

        int LaunchGameRowThunk(unsigned long long titleId, int ejected, unsigned long long heldButtons)
        {
            return App::LaunchGameRow(titleId, ejected != 0, heldButtons) ? 1 : 0;
        }

        void GfxPushLayerThunk(int layer)
        {
            Gfx::PushLayer((Gfx::GfxLayer)layer);
        }

        void GfxPopLayerThunk(void)
        {
            Gfx::PopLayer();
        }

        void GfxPushPanelCornersThunk(float positionX, float positionY, float width, float height,
                                      float cornerTopLeft, float cornerTopRight, float cornerBottomRight, float cornerBottomLeft,
                                      float red, float green, float blue, float alpha)
        {
            Gfx::PushPanelCorners(positionX, positionY, width, height,
                                  cornerTopLeft, cornerTopRight, cornerBottomRight, cornerBottomLeft,
                                  red, green, blue, alpha);
        }

        void GfxPushOverlayQuadThunk(float positionX, float positionY, float width, float height,
                                     float red, float green, float blue, float alpha)
        {
            Gfx::PushOverlayQuad(positionX, positionY, width, height, red, green, blue, alpha);
        }

        void GfxPushOverlayRingThunk(float positionX, float positionY, float width, float height,
                                     float cornerRadius, float thickness)
        {
            Gfx::PushOverlayRing(positionX, positionY, width, height, cornerRadius, thickness);
        }

        int SubmenuDrawTabThunk(int tabIndex)
        {
            Submenus::DrawDynamicTab(tabIndex);
            return 1;
        }

        int SubmenuInputTabThunk(int tabIndex, unsigned long long buttonsDown, unsigned long long buttonsHeld)
        {
            Submenus::InputDynamicTab(tabIndex, buttonsDown, buttonsHeld);
            return 1;
        }

        int TouchRegisterCellThunk(int index, float x, float y, float w, float h)
        {
            return Touch::RegisterCell(index, x, y, w, h) ? 1 : 0;
        }

        int TouchHitTestThunk(int tab, float x, float y, int *indexOut)
        {
            return Touch::HitTest(tab, x, y, indexOut) ? 1 : 0;
        }

        int SubmenuDragThunk(int tabIndex, float moveX, float moveY, int phase)
        {
            return Submenus::Drag(tabIndex, moveX, moveY, phase);
        }

        int SubmenuTouchActionThunk(int tabIndex, int cellIndex)
        {
            return Submenus::TouchAction(tabIndex, cellIndex);
        }

        int SubmenuTapThunk(int tabIndex, float x, float y)
        {
            return Submenus::Tap(tabIndex, x, y);
        }

        int PopupCellAtThunk(float x, float y)
        {
            return WPopup::CellAt(x, y);
        }

        void PopupSelectCellThunk(int index)
        {
            WPopup::SetSelection(index);
        }

        int TakeTapThunk(float *xOut, float *yOut)
        {
            return Touch::TakeTap(xOut, yOut) ? 1 : 0;
        }

        int SubmenuTouchDownThunk(int tabIndex, float x, float y)
        {
            return Submenus::TouchDown(tabIndex, x, y);
        }

        int SubmenuTouchUpThunk(int tabIndex, float x, float y)
        {
            return Submenus::TouchUp(tabIndex, x, y);
        }

        /// @brief Maximum pack asset read (8MB covers fonts and PNG art).
        constexpr unsigned kPackAssetMax = 8u * 1024u * 1024u;

        int PackAssetSizeThunk(const char *assetName)
        {
            if (!assetName || !assetName[0]) {
                return -1;
            }

            unsigned assetSize = 0;
            unsigned char *assetData = Packs::ReadAsset(Packs::ActiveDir(), assetName,
                                                       kPackAssetMax, &assetSize);

            if (!assetData) {
                return -1;
            }

            free(assetData);
            return (int)assetSize;
        }

        int PackAssetReadThunk(const char *assetName, void *outputBuffer, unsigned bufferCapacity)
        {
            if (!assetName || !assetName[0] || !outputBuffer || bufferCapacity == 0) {
                return -1;
            }

            unsigned assetSize = 0;
            unsigned char *assetData = Packs::ReadAsset(Packs::ActiveDir(), assetName,
                                                       bufferCapacity, &assetSize);

            if (!assetData) {
                return -1;
            }

            memcpy(outputBuffer, assetData, assetSize);
            free(assetData);
            return (int)assetSize;
        }

        int SubmenuFullTabIndexThunk(void)
        {
            return Submenus::FullTabIndex();
        }

        int SubmenuTabBackgroundColorThunk(int tabIndex, float *redOut, float *greenOut, float *blueOut)
        {
            return Submenus::TabBackgroundColor(tabIndex, redOut, greenOut, blueOut) ? 1 : 0;
        }

        int TabSetJingleThunk(int tabIndex, const char *jingle)
        {
            return Tabs::SetJingle(tabIndex, jingle) ? 1 : 0;
        }

        int LayoutFindNodeThunk(const char *type, struct LayoutNode *nodeOut)
        {
            return LayoutMod::FindNode(type, nodeOut) ? 1 : 0;
        }

        int LayoutTabCountThunk(void)
        {
            return LayoutMod::TabCount();
        }

        int LayoutTabAtThunk(int slot)
        {
            return LayoutMod::TabAt(slot);
        }

        static const MenuHostApi s_hostApi = {
            kMenuHostApiVersion,
            GfxPushPanelThunk,
            GfxPushIconThunk,
            GfxPushQuadThunk,
            GfxPushSelectRingThunk,
            GfxTexAllocThunk,
            GfxTexValidThunk,
            GfxTexFreeThunk,
            GfxTexUploadRgbaThunk,
            FontDrawThunk,
            FontMeasureThunk,
            FontCenteredThunk,
            ThemeColorThunk,
            ThemeMenuColorThunk,
            ThemeMenuAccentThunk,
            ThemeIsDarkThunk,
            IconsHudSlotThunk,
            LayoutShiftXThunk,
            AppMenuThunk,
            AppTimeThunk,
            SwitchMenuByIdThunk,
            ClockTimeStrThunk,
            ClockBattThunk,
            ClockChargingThunk,
            ClockWifiThunk,
            ClockNetTypeThunk,
            ClockPlaneThunk,
            SfxPlayByNameThunk,
            LogTextThunk,
            TopFocusThunk,
            SetTopFocusThunk,
            SetTopSelectionThunk,
            ThemeCountThunk,
            ThemeNameThunk,
            SetThemeThunk,
            GetThemeThunk,
            ThemePreviewThunk,
            ThemeFileDataThunk,
            ThemeTabColorThunk,
            ThemeTabAccentThunk,
            QextRefreshTitlesThunk,
            QextTitleCountThunk,
            QextTitleIdThunk,
            QextTitleNameThunk,
            QextTitleIconSizeThunk,
            QextTitleIconThunk,
            QextTitleEjectedThunk,
            QextAlbumRefreshThunk,
            QextAlbumCountThunk,
            QextAlbumThumbThunk,
            QextAlbumImageSizeThunk,
            QextAlbumImageThunk,
            QextAlbumIsMovieThunk,
            QextLaunchTitleThunk,
            QextSleepThunk,
            QextLaunchAppletThunk,
            QextGameRunningThunk,
            QextGameHasForegroundThunk,
            QextSuspendedTitleThunk,
            QextBatteryPercentThunk,
            QextWifiSignalThunk,
            QextConnectionTypeThunk,
            QextAirplaneModeThunk,
            GfxTexLockThunk,
            GfxTexUnlockThunk,
            SfxVideoBeginThunk,
            SfxVideoEndThunk,
            SfxVideoPushThunk,
            SfxVideoSeekToThunk,
            SfxVideoBufferedThunk,
            SfxVideoSetPausedThunk,
            QextAlbumMovieOpenThunk,
            QextAlbumMovieSizeThunk,
            QextAlbumMovieReadThunk,
            QextAlbumMovieCloseThunk,
            QextAlbumGrowThunk,
            GfxPushOverlayPanelThunk,
            TopSelectionThunk,
            BottomFocusThunk,
            SetBottomFocusThunk,
            BottomSelectionThunk,
            SetBottomSelectionThunk,
            MenuFromThunk,
            MenuTThunk,
            MenuDirThunk,
            MenuPhaseThunk,
            MenuJingleThunk,
            HomeSubThunk,
            SetHomeSubThunk,
            HomeShiftYThunk,
            HomeShiftBusyThunk,
            RowCountThunk,
            RowAtThunk,
            AppSelectionThunk,
            AppSetSelectionThunk,
            TabFindByIdThunk,
            TabCountThunk,
            TabGlyphThunk,
            TabHudSelectThunk,
            TabHudDeselectThunk,
            TabIdAtThunk,
            ClockAmPmThunk,
            AvatarCountThunk,
            AvatarSlotThunk,
            PadGlyphThunk,
            PadIconColorThunk,
            SubmenuIsDynamicTabThunk,
            SubmenuTabColorThunk,
            WidgetHasThunk,
            WidgetDrawAllThunk,
            IconsSyncTitleThunk,
            FontNextThunk,
            ThemeTryColorThunk,
            PopupOpenThunk,
            PopupCloseThunk,
            PopupIsOpenThunk,
            PopupSelectionThunk,
            PopupInputThunk,
            PopupSetGridColumnsThunk,
            RefreshTitlesThunk,
            LaunchGameRowThunk,
            GfxPushLayerThunk,
            GfxPopLayerThunk,
            GfxPushPanelCornersThunk,
            GfxPushOverlayQuadThunk,
            GfxPushOverlayRingThunk,
            SubmenuDrawTabThunk,
            SubmenuInputTabThunk,
            SubmenuFullTabIndexThunk,
            SubmenuTabBackgroundColorThunk,
            TabSetJingleThunk,
            LayoutFindNodeThunk,
            AppTopSwitchThunk,
            AppActivateBottomThunk,
            AppActivateThunk,
            PromptOpenGameThunk,
            OpenFolderViewThunk,
            CloseFolderViewThunk,
            OpenFolderThunk,
            FolderSelThunk,
            SetFolderSelThunk,
            SelectFolderRowThunk,
            LayoutTabCountThunk,
            LayoutTabAtThunk,
            MenuSlideXThunk,
            TouchRegisterCellThunk,
            TouchHitTestThunk,
            SubmenuDragThunk,
            SubmenuTouchActionThunk,
            SubmenuTapThunk,
            PopupCellAtThunk,
            PopupSelectCellThunk,
            TakeTapThunk,
            SubmenuTouchDownThunk,
            SubmenuTouchUpThunk,
            PackAssetSizeThunk,
            PackAssetReadThunk,
        };

    } // namespace

    /// @brief Give the menu host API table.
    /// @return Host table with all menu functions plus qext.
    const MenuHostApi *GetHostApi()
    {
        return &s_hostApi;
    }

} /* namespace HostApi */
