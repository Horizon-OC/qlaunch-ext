/* NekoMenu Settings submenu (C) 2026 Souldbminer */
/* Licensed under the GPLv2                 */

#include <switch.h>
#include "MenuHost.hpp"
#include "SubmenuApi.hpp"
#include <cstdio>
#include <cstring>
#include <stdint.h>

extern const uint8_t top_sel_settings_png[];
extern const uint32_t top_sel_settings_png_size;
extern const uint8_t top_des_settings_png[];
extern const uint32_t top_des_settings_png_size;

namespace {

    constexpr int kSettingsOptionCount = 18;
    constexpr int kSettingsMaxToggles = 8;
    constexpr int kSettingsThemesPage = 11;
    constexpr int kSettingsNoPage = -1;
    constexpr int kSettingsRowLabel = 0;
    constexpr int kSettingsRowToggle = 1;
    constexpr int kSettingsRowAction = 2;
    constexpr int kSettingsActionControllers = 1;
    constexpr int kSettingsActionMii = 2;
    constexpr unsigned kSettingsRepeatHold = 20;
    constexpr unsigned kSettingsRepeatPeriod = 6;

    const char *kSettingsOptions[kSettingsOptionCount] = {
        "Controller", "Health", "Airplane", "Bright", "Internet", "BT",
        "TV", "Data", "Lock", "Parent", "Accessibility", "Themes",
        "Users", "Mii", "amiibo", "Noti", "Sleep", "System",
    };

    struct SettingsRow {
        const char *labelText;
        int rowKind;
        int actionId;
    };

    const SettingsRow kSettingsPageController[] = {
        { "Change Grip/Order", kSettingsRowAction, kSettingsActionControllers },
        { "Pro Controller Wired Communication", kSettingsRowToggle, 0 },
        { "NFC Function", kSettingsRowToggle, 1 },
        { "Vibration", kSettingsRowToggle, 2 },
        { "Calibrate Control Sticks", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageHealth[] = {
        { "Safety Guide", kSettingsRowLabel, 0 },
        { "Precautions", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageAirplane[] = {
        { "Airplane Mode", kSettingsRowToggle, 0 },
        { "Bluetooth in Airplane Mode", kSettingsRowToggle, 1 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageBrightness[] = {
        { "Screen Brightness", kSettingsRowLabel, 0 },
        { "Auto-Brightness", kSettingsRowToggle, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageInternet[] = {
        { "Internet Settings", kSettingsRowLabel, 0 },
        { "Connection Status", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageBluetooth[] = {
        { "Bluetooth Audio", kSettingsRowToggle, 0 },
        { "Pair New Device", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageTelevision[] = {
        { "TV Resolution", kSettingsRowLabel, 0 },
        { "Match TV Power State", kSettingsRowToggle, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageData[] = {
        { "System Memory", kSettingsRowLabel, 0 },
        { "microSD Card", kSettingsRowLabel, 0 },
        { "Manage Software", kSettingsRowLabel, 0 },
        { "Manage Screenshots and Videos", kSettingsRowLabel, 0 },
        { "Cloud Backup", kSettingsRowLabel, 0 },
        { "Transfer to System Memory", kSettingsRowLabel, 0 },
        { "Format microSD Card", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageLock[] = {
        { "Console Screen Lock", kSettingsRowToggle, 0 },
        { "Change PIN", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageParental[] = {
        { "Parental Controls Settings", kSettingsRowLabel, 0 },
        { "Restriction Level", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageAccessibility[] = {
        { "Zoom", kSettingsRowToggle, 0 },
        { "Grayscale", kSettingsRowToggle, 1 },
        { "Invert Colors", kSettingsRowToggle, 2 },
        { "Button Mapping", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageUsers[] = {
        { "Add User", kSettingsRowLabel, 0 },
        { "User Settings", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageMii[] = {
        { "Create Mii", kSettingsRowAction, kSettingsActionMii },
        { "Edit Mii", kSettingsRowAction, kSettingsActionMii },
        { "Delete Mii", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageAmiibo[] = {
        { "Register Owner and Nickname", kSettingsRowLabel, 0 },
        { "Delete Game Data", kSettingsRowLabel, 0 },
        { "Reset amiibo", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageNotifications[] = {
        { "Notification Display", kSettingsRowToggle, 0 },
        { "Download Notifications", kSettingsRowToggle, 1 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageSleep[] = {
        { "Auto-Sleep", kSettingsRowToggle, 0 },
        { "Maintain Internet Connection in Sleep", kSettingsRowToggle, 1 },
        { "Wake When AC Adapter Is Unplugged", kSettingsRowToggle, 2 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageSystem[] = {
        { "System Update", kSettingsRowLabel, 0 },
        { "Date and Time", kSettingsRowLabel, 0 },
        { "Language", kSettingsRowLabel, 0 },
        { "Console Nickname", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    enum class SettingsMode {
        OptionList,
        Detail,
    };

    struct SettingsState {
        int pageIndex;
        int rowIndex;
        bool toggleStates[kSettingsOptionCount][kSettingsMaxToggles];
        unsigned repeatCounter;
    };

    static SettingsState s_state = { kSettingsNoPage, 0, {}, 0 };

    /// @brief Current settings mode.
    /// @return OptionList when no page is open, else Detail.
    SettingsMode CurrentMode()
    {
        return s_state.pageIndex < 0 ? SettingsMode::OptionList : SettingsMode::Detail;
    }

    /// @brief Look up the row table for a detail page.
    /// @param pageIndex Settings page index.
    /// @return Row table or null.
    const SettingsRow *SettingsPageRows(int pageIndex)
    {
        switch (pageIndex) {
            case 0:
                return kSettingsPageController;
            case 1:
                return kSettingsPageHealth;
            case 2:
                return kSettingsPageAirplane;
            case 3:
                return kSettingsPageBrightness;
            case 4:
                return kSettingsPageInternet;
            case 5:
                return kSettingsPageBluetooth;
            case 6:
                return kSettingsPageTelevision;
            case 7:
                return kSettingsPageData;
            case 8:
                return kSettingsPageLock;
            case 9:
                return kSettingsPageParental;
            case 10:
                return kSettingsPageAccessibility;
            case 12:
                return kSettingsPageUsers;
            case 13:
                return kSettingsPageMii;
            case 14:
                return kSettingsPageAmiibo;
            case 15:
                return kSettingsPageNotifications;
            case 16:
                return kSettingsPageSleep;
            case 17:
                return kSettingsPageSystem;
            default:
                return nullptr;
        }
    }

    /// @brief Count rows in a null-terminated settings page.
    /// @param pageRows Page rows.
    /// @return Row count.
    int SettingsRowCount(const SettingsRow *pageRows)
    {
        int rowCount = 0;

        while (pageRows && pageRows[rowCount].labelText) {
            rowCount++;
        }

        return rowCount;
    }

    /// @brief Poll directional buttons with key repeat.
    /// @param buttonsDown Pressed mask, updated with repeats.
    /// @param buttonsHeld Held mask.
    void PollRepeat(unsigned long long &buttonsDown, unsigned long long buttonsHeld)
    {
        constexpr unsigned long long directionMask =
            (unsigned long long)HidNpadButton_AnyLeft | (unsigned long long)HidNpadButton_AnyRight |
            (unsigned long long)HidNpadButton_AnyUp | (unsigned long long)HidNpadButton_AnyDown;
        unsigned long long heldDirections = buttonsHeld & directionMask;

        if (buttonsDown & directionMask) {
            s_state.repeatCounter = 0;
            return;
        }

        if (heldDirections) {
            s_state.repeatCounter++;

            if (s_state.repeatCounter > kSettingsRepeatHold &&
                (s_state.repeatCounter % kSettingsRepeatPeriod) == 0) {
                buttonsDown |= heldDirections;
            }
        } else {
            s_state.repeatCounter = 0;
        }
    }

    /// @brief Draw the left settings option list.
    /// @param hostApi Host API.
    /// @param slideOffsetX Horizontal slide offset.
    void DrawOptions(const MenuHostApi *hostApi, float slideOffsetX)
    {
        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        hostApi->themeColor("ink", &inkRed, &inkGreen, &inkBlue);

        float dimRed = 0.0f;
        float dimGreen = 0.0f;
        float dimBlue = 0.0f;
        hostApi->themeColor("dim", &dimRed, &dimGreen, &dimBlue);

        float listTopY = 258.0f;
        float rowHeight = 34.0f;

        for (int optionIndex = 0; optionIndex < kSettingsOptionCount; optionIndex++) {
            float rowY = listTopY + (float)optionIndex * rowHeight;
            bool isSelected = (s_state.pageIndex < 0 && s_state.rowIndex == optionIndex);

            if (isSelected) {
                float accentRed = 0.0f;
                float accentGreen = 0.0f;
                float accentBlue = 0.0f;
                hostApi->themeColor("accent", &accentRed, &accentGreen, &accentBlue);
                hostApi->gfxPushPanel(110.0f + slideOffsetX, rowY - 4.0f, 500.0f, rowHeight, 8.0f,
                                      accentRed, accentGreen, accentBlue, 1.0f);
            }

            hostApi->fontDraw(kSettingsOptions[optionIndex], 130.0f + slideOffsetX, rowY + 22.0f, 27.0f,
                              isSelected ? 1.0f : (s_state.pageIndex == optionIndex ? inkRed : dimRed),
                              isSelected ? 1.0f : (s_state.pageIndex == optionIndex ? inkGreen : dimGreen),
                              isSelected ? 1.0f : (s_state.pageIndex == optionIndex ? inkBlue : dimBlue),
                              1.0f);
        }
    }

    /// @brief Draw a toggle row ON/OFF value.
    /// @param hostApi Host API.
    /// @param pageIndex Settings page index.
    /// @param rowIndex Row index inside the page.
    /// @param rightEdgeX Right edge X.
    /// @param baselineY Baseline Y.
    void DrawToggleValue(const MenuHostApi *hostApi, int pageIndex, int rowIndex, float rightEdgeX, float baselineY)
    {
        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        hostApi->themeColor("ink", &inkRed, &inkGreen, &inkBlue);

        float dimRed = 0.0f;
        float dimGreen = 0.0f;
        float dimBlue = 0.0f;
        hostApi->themeColor("dim", &dimRed, &dimGreen, &dimBlue);

        bool isEnabled = (pageIndex >= 0 && pageIndex < kSettingsOptionCount && rowIndex >= 0 && rowIndex < kSettingsMaxToggles)
            ? s_state.toggleStates[pageIndex][rowIndex]
            : false;
        const char *toggleText = isEnabled ? "ON" : "OFF";
        float textWidth = hostApi->fontMeasure(toggleText, 30.0f);
        hostApi->fontDraw(toggleText, rightEdgeX - textWidth, baselineY, 30.0f,
                          isEnabled ? inkRed : dimRed, isEnabled ? inkGreen : dimGreen,
                          isEnabled ? inkBlue : dimBlue, 1.0f);
    }

    /// @brief Draw the theme picker detail page.
    /// @param hostApi Host API.
    /// @param slideOffsetX Horizontal slide offset.
    void DrawThemesDetail(const MenuHostApi *hostApi, float slideOffsetX)
    {
        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        hostApi->themeColor("ink", &inkRed, &inkGreen, &inkBlue);

        int themeCount = hostApi->themeCount();
        int activeTheme = hostApi->getTheme();
        float cardWidth = 520.0f;
        float cardHeight = 64.0f;
        float cardGap = 24.0f;

        for (int themeIndex = 0; themeIndex < themeCount; themeIndex++) {
            int gridColumn = themeIndex % 2;
            int gridRow = themeIndex / 2;
            float cardX = 700.0f + slideOffsetX + gridColumn * (cardWidth + cardGap);
            float cardY = 300.0f + gridRow * (cardHeight + 12.0f);
            bool isSelected = (s_state.rowIndex == themeIndex);

            float previewRed = 0.0f;
            float previewGreen = 0.0f;
            float previewBlue = 0.0f;
            hostApi->themePreview(themeIndex, &previewRed, &previewGreen, &previewBlue);
            hostApi->gfxPushPanel(cardX, cardY, cardWidth, cardHeight, 12.0f,
                                  previewRed, previewGreen, previewBlue, 1.0f);

            if (themeIndex == activeTheme || isSelected) {
                float accentRed = 0.0f;
                float accentGreen = 0.0f;
                float accentBlue = 0.0f;
                hostApi->themeColor("accent", &accentRed, &accentGreen, &accentBlue);
                hostApi->gfxPushPanel(cardX - 4.0f, cardY - 4.0f, cardWidth + 8.0f, cardHeight + 8.0f,
                                      14.0f, accentRed, accentGreen, accentBlue, 0.35f);
            }

            const char *themeName = hostApi->themeName(themeIndex);
            float nameWidth = hostApi->fontMeasure(themeName, 30.0f);
            hostApi->fontDraw(themeName, cardX + (cardWidth - nameWidth) * 0.5f, cardY + 42.0f, 30.0f,
                              inkRed, inkGreen, inkBlue, 1.0f);
        }
    }

    /// @brief Draw the right detail pane.
    /// @param hostApi Host API.
    /// @param slideOffsetX Horizontal slide offset.
    void DrawDetail(const MenuHostApi *hostApi, float slideOffsetX)
    {
        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        hostApi->themeColor("ink", &inkRed, &inkGreen, &inkBlue);

        float dimRed = 0.0f;
        float dimGreen = 0.0f;
        float dimBlue = 0.0f;
        hostApi->themeColor("dim", &dimRed, &dimGreen, &dimBlue);

        if (s_state.pageIndex < 0 || s_state.pageIndex >= kSettingsOptionCount) {
            return;
        }

        hostApi->fontDraw(kSettingsOptions[s_state.pageIndex], 700.0f + slideOffsetX, 250.0f, 40.0f,
                          inkRed, inkGreen, inkBlue, 1.0f);

        if (s_state.pageIndex == kSettingsThemesPage) {
            DrawThemesDetail(hostApi, slideOffsetX);
            return;
        }

        const SettingsRow *pageRows = SettingsPageRows(s_state.pageIndex);
        int rowCount = SettingsRowCount(pageRows);
        float detailTopY = 320.0f;
        float detailRowHeight = 64.0f;

        for (int detailIndex = 0; detailIndex < rowCount; detailIndex++) {
            float detailY = detailTopY + (float)detailIndex * detailRowHeight;
            bool isSelected = (s_state.rowIndex == detailIndex);

            if (isSelected) {
                float accentRed = 0.0f;
                float accentGreen = 0.0f;
                float accentBlue = 0.0f;
                hostApi->themeColor("accent", &accentRed, &accentGreen, &accentBlue);
                hostApi->gfxPushPanel(690.0f + slideOffsetX, detailY - 6.0f, 1100.0f, detailRowHeight, 12.0f,
                                      accentRed, accentGreen, accentBlue, 0.30f);
            }

            float labelRed = isSelected ? inkRed : dimRed;
            float labelGreen = isSelected ? inkGreen : dimGreen;
            float labelBlue = isSelected ? inkBlue : dimBlue;

            hostApi->fontDraw(pageRows[detailIndex].labelText, 720.0f + slideOffsetX, detailY + 40.0f, 32.0f,
                              labelRed, labelGreen, labelBlue, 1.0f);

            if (pageRows[detailIndex].rowKind == kSettingsRowToggle) {
                DrawToggleValue(hostApi, s_state.pageIndex, detailIndex, 1760.0f + slideOffsetX, detailY + 40.0f);
            }
        }
    }

    /// @brief Activate the focused detail row.
    /// @param hostApi Host API.
    void ActivateDetailRow(const MenuHostApi *hostApi)
    {
        if (s_state.pageIndex == kSettingsThemesPage) {
            hostApi->setTheme(s_state.rowIndex);
            return;
        }

        const SettingsRow *pageRows = SettingsPageRows(s_state.pageIndex);

        if (!pageRows || !pageRows[s_state.rowIndex].labelText) {
            return;
        }

        if (pageRows[s_state.rowIndex].rowKind == kSettingsRowToggle) {
            if (s_state.rowIndex >= 0 && s_state.rowIndex < kSettingsMaxToggles) {
                s_state.toggleStates[s_state.pageIndex][s_state.rowIndex] =
                    !s_state.toggleStates[s_state.pageIndex][s_state.rowIndex];
            }
        } else if (pageRows[s_state.rowIndex].rowKind == kSettingsRowAction) {
            if (pageRows[s_state.rowIndex].actionId == kSettingsActionControllers) {
                hostApi->qextLaunchApplet(1);
            } else if (pageRows[s_state.rowIndex].actionId == kSettingsActionMii) {
                hostApi->qextLaunchApplet(2);
            }
        }
    }

    /// @brief Handle option list input.
    /// @param hostApi Host API.
    /// @param buttonsDown Buttons pressed.
    void HandleOptionListInput(const MenuHostApi *hostApi, unsigned long long buttonsDown)
    {
        if (buttonsDown & (unsigned long long)HidNpadButton_AnyUp) {
            int nextRow = (s_state.rowIndex + kSettingsOptionCount - 1) % kSettingsOptionCount;

            if (nextRow != s_state.rowIndex) {
                hostApi->sfxPlayByName("hover");
            }

            s_state.rowIndex = nextRow;
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_AnyDown) {
            int nextRow = (s_state.rowIndex + 1) % kSettingsOptionCount;

            if (nextRow != s_state.rowIndex) {
                hostApi->sfxPlayByName("hover");
            }

            s_state.rowIndex = nextRow;
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_A) {
            s_state.pageIndex = s_state.rowIndex;
            s_state.rowIndex = 0;
            hostApi->sfxPlayByName("click");
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_B) {
            hostApi->switchMenuById("home");
            hostApi->sfxPlayByName("back");
        }
    }

    /// @brief Handle detail page input.
    /// @param hostApi Host API.
    /// @param buttonsDown Buttons pressed.
    void HandleDetailInput(const MenuHostApi *hostApi, unsigned long long buttonsDown)
    {
        int rowCount = (s_state.pageIndex == kSettingsThemesPage) ? hostApi->themeCount()
                         : SettingsRowCount(SettingsPageRows(s_state.pageIndex));

        if (rowCount <= 0) {
            rowCount = 1;
        }

        int columnCount = (s_state.pageIndex == kSettingsThemesPage) ? 2 : 1;

        if (buttonsDown & (unsigned long long)HidNpadButton_AnyLeft) {
            int nextRow = (s_state.rowIndex + rowCount - 1) % rowCount;

            if (nextRow != s_state.rowIndex) {
                hostApi->sfxPlayByName("hover");
            }

            s_state.rowIndex = nextRow;
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_AnyRight) {
            int nextRow = (s_state.rowIndex + 1) % rowCount;

            if (nextRow != s_state.rowIndex) {
                hostApi->sfxPlayByName("hover");
            }

            s_state.rowIndex = nextRow;
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_AnyUp) {
            int nextRow = (s_state.rowIndex + rowCount - columnCount) % rowCount;

            if (nextRow != s_state.rowIndex) {
                hostApi->sfxPlayByName("hover");
            }

            s_state.rowIndex = nextRow;
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_AnyDown) {
            int nextRow = (s_state.rowIndex + columnCount) % rowCount;

            if (nextRow != s_state.rowIndex) {
                hostApi->sfxPlayByName("hover");
            }

            s_state.rowIndex = nextRow;
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_A) {
            ActivateDetailRow(hostApi);
            hostApi->sfxPlayByName("click");
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_B) {
            s_state.pageIndex = kSettingsNoPage;
            s_state.rowIndex = 0;
            hostApi->sfxPlayByName("back");
        }
    }

} // namespace

extern "C" {

    /// @brief Give the submenu ABI version.
    /// @return kSubmenuApiVersion.
    QLAUNCH_HOOK int SubmenuApiVersion(void)
    {
        return kSubmenuApiVersion;
    }

    /// @brief Give the stable submenu id.
    /// @return Lowercase id owned by the dnro.
    QLAUNCH_HOOK const char *SubmenuId(void)
    {
        return "settings";
    }

    /// @brief Give the display title.
    /// @return Title owned by the dnro.
    QLAUNCH_HOOK const char *SubmenuTitle(void)
    {
        return "Settings";
    }

    /// @brief Give the jingle clip name, if any.
    /// @return Clip name or NULL.
    QLAUNCH_HOOK const char *SubmenuJingle(void)
    {
        return "settings_jingle";
    }

    /// @brief Give the fallback glyph, if any.
    /// @return Codepoint or 0.
    QLAUNCH_HOOK unsigned SubmenuGlyph(void)
    {
        return 0xE130;
    }

    /// @brief Give the embedded icon PNG bytes, if any.
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL.
    QLAUNCH_HOOK const unsigned char *SubmenuIconPng(unsigned *outputSize)
    {
        if (outputSize) {
            *outputSize = top_sel_settings_png_size;
        }

        return top_sel_settings_png;
    }

    /// @brief Give the embedded selected-state icon PNG bytes, if any.
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL to reuse the normal icon.
    QLAUNCH_HOOK const unsigned char *SubmenuSelectedIconPng(unsigned *outputSize)
    {
        if (outputSize) {
            *outputSize = top_des_settings_png_size;
        }

        return top_des_settings_png;
    }

    /// @brief Give the submenu accent color.
    /// @param hostApi Host API for theme-derived colors.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    /// @return Nonzero when a color is provided.
    QLAUNCH_HOOK int SubmenuColor(const MenuHostApi *hostApi,
                                  float *redOut, float *greenOut, float *blueOut)
    {
        if (!hostApi || !redOut || !greenOut || !blueOut) {
            return 0;
        }

        hostApi->themeTabAccent("settings", redOut, greenOut, blueOut);
        return 1;
    }

    /// @brief Give the submenu background color.
    /// @param hostApi Host API for theme-derived colors.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    /// @return Nonzero when a color is provided.
    QLAUNCH_HOOK int SubmenuBackgroundColor(const MenuHostApi *hostApi,
                                            float *redOut, float *greenOut, float *blueOut)
    {
        if (!hostApi || !redOut || !greenOut || !blueOut) {
            return 0;
        }

        hostApi->themeTabColor("settings", redOut, greenOut, blueOut);
        return 1;
    }

    /// @brief Boot the submenu with the menu host API.
    /// @param hostApi All menu functions plus the qext family.
    /// @return 0 to stay loaded.
    QLAUNCH_HOOK int SubmenuBoot(const MenuHostApi *hostApi)
    {
        if (!hostApi || hostApi->version != kMenuHostApiVersion) {
            return -1;
        }

        s_state.pageIndex = kSettingsNoPage;
        s_state.rowIndex = 0;
        s_state.repeatCounter = 0;
        memset(s_state.toggleStates, 0, sizeof(s_state.toggleStates));
        return 0;
    }

    /// @brief Draw the submenu onto the core's state.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuDraw(const MenuHostApi *hostApi)
    {
        float slideOffsetX = hostApi->layoutShiftX();
        DrawOptions(hostApi, slideOffsetX);
        DrawDetail(hostApi, slideOffsetX);
    }

    /// @brief Handle submenu input.
    /// @param hostApi Host API.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    QLAUNCH_HOOK void SubmenuInput(const MenuHostApi *hostApi,
                                   unsigned long long buttonsDown,
                                   unsigned long long buttonsHeld)
    {
        PollRepeat(buttonsDown, buttonsHeld);

        switch (CurrentMode()) {
            case SettingsMode::OptionList:
                HandleOptionListInput(hostApi, buttonsDown);
                break;

            case SettingsMode::Detail:
                HandleDetailInput(hostApi, buttonsDown);
                break;
        }
    }

    /// @brief Enter the submenu tab.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuOpen(const MenuHostApi *hostApi)
    {
        (void)hostApi;
    }

    /// @brief Exit the submenu tab.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuClose(const MenuHostApi *hostApi)
    {
        (void)hostApi;
    }

    /// @brief Tick the submenu once per frame.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuTick(const MenuHostApi *hostApi)
    {
        (void)hostApi;
    }

    /// @brief Shut down before unload.
    QLAUNCH_HOOK void SubmenuShutdown(void)
    {
    }

} /* extern C */
