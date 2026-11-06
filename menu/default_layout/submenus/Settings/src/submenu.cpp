/* NekoMenu Settings submenu (C) 2026 Souldbminer */
/* Licensed under the GPLv2                 */

#include <switch.h>
#include "MenuHost.hpp"
#include "SubmenuApi.hpp"
#include "stb_image.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>

extern const uint8_t top_sel_settings_png[];
extern const uint32_t top_sel_settings_png_size;
extern const uint8_t top_des_settings_png[];
extern const uint32_t top_des_settings_png_size;

extern const uint8_t set_00_png[];
extern const uint32_t set_00_png_size;
extern const uint8_t set_01_png[];
extern const uint32_t set_01_png_size;
extern const uint8_t set_02_png[];
extern const uint32_t set_02_png_size;
extern const uint8_t set_03_png[];
extern const uint32_t set_03_png_size;
extern const uint8_t set_04_png[];
extern const uint32_t set_04_png_size;
extern const uint8_t set_05_png[];
extern const uint32_t set_05_png_size;
extern const uint8_t set_06_png[];
extern const uint32_t set_06_png_size;
extern const uint8_t set_07_png[];
extern const uint32_t set_07_png_size;
extern const uint8_t set_08_png[];
extern const uint32_t set_08_png_size;
extern const uint8_t set_09_png[];
extern const uint32_t set_09_png_size;
extern const uint8_t set_10_png[];
extern const uint32_t set_10_png_size;
extern const uint8_t set_11_png[];
extern const uint32_t set_11_png_size;
extern const uint8_t set_12_png[];
extern const uint32_t set_12_png_size;
extern const uint8_t set_13_png[];
extern const uint32_t set_13_png_size;
extern const uint8_t set_14_png[];
extern const uint32_t set_14_png_size;
extern const uint8_t set_15_png[];
extern const uint32_t set_15_png_size;
extern const uint8_t set_16_png[];
extern const uint32_t set_16_png_size;
extern const uint8_t set_17_png[];
extern const uint32_t set_17_png_size;

namespace {

    constexpr int kSettingsOptionCount = 18;
    constexpr int kSettingsThemesPage = 11;
    constexpr int kSettingsNoPage = -1;
    constexpr int kSettingsRowLabel = 0;
    constexpr int kSettingsRowAction = 1;
    constexpr int kSettingsActionControllers = 1;
    constexpr int kSettingsActionMii = 2;
    constexpr unsigned kSettingsRepeatHold = 20;
    constexpr unsigned kSettingsRepeatPeriod = 6;
    constexpr int kSettingsIconMax = 96;

    const char *kSettingsOptions[kSettingsOptionCount] = {
        "Controller", "Health", "Airplane", "Bright", "Internet", "BT",
        "TV", "Data", "Lock", "Parent", "Accessibility", "Themes",
        "Users", "Mii", "amiibo", "Noti", "Sleep", "System",
    };

    /// @brief Detail pane titles, taken from the mockup `setting` labels.
    const char *kSettingsTitles[kSettingsOptionCount] = {
        "Controller Settings", "Support/Health & Safety", "Airplane Mode",
        "Screen Brightness", "Internet", "Audio", "Display", "Software Data",
        "Screen Lock", "Parental Controls", "Accessibility", "Themes",
        "Users", "Mii", "amiibo", "Notification", "Sleep Mode", "System",
    };

    struct SettingsRow {
        const char *labelText;
        int rowKind;
        int actionId;
    };

    /* Detail rows follow the mockup sub-menu content in column-major
       order (column 0 rows first, then column 1). */
    const SettingsRow kSettingsPageController[] = {
        { "Change Grip/Order", kSettingsRowAction, kSettingsActionControllers },
        { "Search for Controllers", kSettingsRowLabel, 0 },
        { "How to Pair Controllers", kSettingsRowLabel, 0 },
        { "How to Use GL/GR Buttons", kSettingsRowLabel, 0 },
        { "How to Use Mouse Controls", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageHealth[] = {
        { "Health and Support Information", kSettingsRowLabel, 0 },
        { "Error History", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageInternet[] = {
        { "Internet Settings", kSettingsRowLabel, 0 },
        { "Test Connection", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageBluetooth[] = {
        { "Output Test", kSettingsRowLabel, 0 },
        { "Input Test", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageTelevision[] = {
        { "Adjust HDR", kSettingsRowLabel, 0 },
        { "Adjust Screen Size", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageData[] = {
        { "Move System / microSD Card Data", kSettingsRowLabel, 0 },
        { "Quick Delete", kSettingsRowLabel, 0 },
        { "Software", kSettingsRowLabel, 0 },
        { "Manage Screenshots and Videos", kSettingsRowLabel, 0 },
        { "Save Data Cloud", kSettingsRowLabel, 0 },
        { "Transfer Your Save Data", kSettingsRowLabel, 0 },
        { "Delete Save Data", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageParental[] = {
        { "Parental Controls", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageAccessibility[] = {
        { "Button Mapping", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageUsers[] = {
        { "Add User", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageMii[] = {
        { "Create a Mii", kSettingsRowAction, kSettingsActionMii },
        { "Edit a Mii", kSettingsRowLabel, 0 },
        { "Send/Recieve a Mii", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageAmiibo[] = {
        { "Enter Owner and Nickname", kSettingsRowLabel, 0 },
        { "Delete Game Data", kSettingsRowLabel, 0 },
        { "Reset amiibo", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageNotifications[] = {
        { "Alarm Notifications", kSettingsRowLabel, 0 },
        { "Platinum Point Notifications", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    const SettingsRow kSettingsPageSystem[] = {
        { "System Update", kSettingsRowLabel, 0 },
        { "Date and Time", kSettingsRowLabel, 0 },
        { "Dock Update", kSettingsRowLabel, 0 },
        { "System Information", kSettingsRowLabel, 0 },
        { nullptr, 0, 0 },
    };

    struct SettingsIconBin {
        const uint8_t *pngData;
        const uint32_t *pngSize;
    };

    static const SettingsIconBin kSettingsOptionIcons[kSettingsOptionCount] = {
        { set_00_png, &set_00_png_size }, { set_01_png, &set_01_png_size },
        { set_02_png, &set_02_png_size }, { set_03_png, &set_03_png_size },
        { set_04_png, &set_04_png_size }, { set_05_png, &set_05_png_size },
        { set_06_png, &set_06_png_size }, { set_07_png, &set_07_png_size },
        { set_08_png, &set_08_png_size }, { set_09_png, &set_09_png_size },
        { set_10_png, &set_10_png_size }, { set_11_png, &set_11_png_size },
        { set_12_png, &set_12_png_size }, { set_13_png, &set_13_png_size },
        { set_14_png, &set_14_png_size }, { set_15_png, &set_15_png_size },
        { set_16_png, &set_16_png_size }, { set_17_png, &set_17_png_size },
    };

    enum class SettingsMode {
        OptionList,
        Detail,
    };

    struct SettingsState {
        int pageIndex;
        int rowIndex;
        int optionIconSlots[kSettingsOptionCount];
        unsigned repeatCounter;
    };

    static SettingsState s_state = { kSettingsNoPage, 0, { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, 0 };
    static const MenuHostApi *s_hostApi = nullptr;

    /// @brief Current mode.
    /// @return OptionList when no page is open, else Detail.
    SettingsMode CurrentMode()
    {
        return s_state.pageIndex < 0 ? SettingsMode::OptionList : SettingsMode::Detail;
    }

    /// @brief Look up the row table for a detail page (column-major).
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
            case 3:
                return nullptr;
            case 4:
                return kSettingsPageInternet;
            case 5:
                return kSettingsPageBluetooth;
            case 6:
                return kSettingsPageTelevision;
            case 7:
                return kSettingsPageData;
            case 8:
                return nullptr;
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
                return nullptr;
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

    /// @brief Column count for a detail page (themes grid is separate).
    /// @param pageIndex Settings page index.
    /// @return 2 when the mockup lays the page out in two columns, else 1.
    int SettingsPageColumns(int pageIndex)
    {
        switch (pageIndex) {
            case 1:
            case 4:
            case 5:
            case 6:
            case 7:
            case 13:
            case 15:
            case 17:
                return 2;
            default:
                return 1;
        }
    }

    /// @brief Row count inside one column of a two-column page.
    /// Columns are filled left to right.
    /// @param pageIndex Settings page index.
    /// @param columnIndex Column index (0 or 1).
    /// @return Row count in that column.
    int SettingsPageColumnRows(int pageIndex, int columnIndex)
    {
        const SettingsRow *pageRows = SettingsPageRows(pageIndex);
        int rowCount = SettingsRowCount(pageRows);

        if (SettingsPageColumns(pageIndex) < 2) {
            return columnIndex == 0 ? rowCount : 0;
        }

        int leftCount = (rowCount + 1) / 2;
        return columnIndex == 0 ? leftCount : rowCount - leftCount;
    }

    /// @brief Split a column-major flat index into column and row.
    /// @param pageIndex Settings page index.
    /// @param flatIndex Column-major flat index.
    /// @param columnOut Column output.
    /// @param rowOut Row output.
    void SettingsCellSplit(int pageIndex, int flatIndex, int *columnOut, int *rowOut)
    {
        int leftCount = SettingsPageColumnRows(pageIndex, 0);

        if (flatIndex < leftCount) {
            *columnOut = 0;
            *rowOut = flatIndex;
        } else {
            *columnOut = 1;
            *rowOut = flatIndex - leftCount;
        }
    }

    /// @brief Join a column and row into a column-major flat index.
    /// @param pageIndex Settings page index.
    /// @param columnIndex Column index (0 or 1, clamped).
    /// @param rowIndex Row index (clamped into the column).
    /// @return Flat index.
    int SettingsCellJoin(int pageIndex, int columnIndex, int rowIndex)
    {
        int leftCount = SettingsPageColumnRows(pageIndex, 0);
        int rightCount = SettingsPageColumnRows(pageIndex, 1);

        if (columnIndex <= 0 || rightCount <= 0) {
            if (rowIndex >= leftCount) {
                rowIndex = leftCount - 1;
            }

            if (rowIndex < 0) {
                rowIndex = 0;
            }

            return rowIndex;
        }

        if (rowIndex >= rightCount) {
            rowIndex = rightCount - 1;
        }

        if (rowIndex < 0) {
            rowIndex = 0;
        }

        return leftCount + rowIndex;
    }

    /// @brief Downscale RGBA art to the icon budget with a box filter.
    /// @param hostApi Host API (unused, kept for symmetry).
    /// @param pixelData Source pixels, freed or adopted by the caller.
    /// @param width Source width (updated to the scaled width).
    /// @param height Source height (updated to the scaled height).
    /// @return Scaled pixels (caller frees), or null on failure.
    unsigned char *DownscaleIcon(const MenuHostApi *hostApi, unsigned char *pixelData, int *width, int *height)
    {
        (void)hostApi;

        int sourceWidth = *width;
        int sourceHeight = *height;
        int targetWidth = sourceWidth;
        int targetHeight = sourceHeight;

        while (targetWidth > kSettingsIconMax || targetHeight > kSettingsIconMax) {
            targetWidth /= 2;
            targetHeight /= 2;
        }

        if (targetWidth == sourceWidth && targetHeight == sourceHeight) {
            return pixelData;
        }

        unsigned char *scaled = (unsigned char *)malloc((size_t)targetWidth * (size_t)targetHeight * 4u);

        if (!scaled) {
            return pixelData;
        }

        for (int y = 0; y < targetHeight; y++) {
            int y0 = y * sourceHeight / targetHeight;
            int y1 = (y + 1) * sourceHeight / targetHeight;

            if (y1 <= y0) {
                y1 = y0 + 1;
            }

            for (int x = 0; x < targetWidth; x++) {
                int x0 = x * sourceWidth / targetWidth;
                int x1 = (x + 1) * sourceWidth / targetWidth;

                if (x1 <= x0) {
                    x1 = x0 + 1;
                }

                unsigned sumRed = 0, sumGreen = 0, sumBlue = 0, sumAlpha = 0, count = 0;

                for (int sy = y0; sy < y1; sy++) {
                    for (int sx = x0; sx < x1; sx++) {
                        unsigned char *pixel = pixelData + ((size_t)sy * (size_t)sourceWidth + (size_t)sx) * 4u;
                        sumRed += pixel[0];
                        sumGreen += pixel[1];
                        sumBlue += pixel[2];
                        sumAlpha += pixel[3];
                        count++;
                    }
                }

                unsigned char *out = scaled + ((size_t)y * (size_t)targetWidth + (size_t)x) * 4u;
                out[0] = (unsigned char)(sumRed / count);
                out[1] = (unsigned char)(sumGreen / count);
                out[2] = (unsigned char)(sumBlue / count);
                out[3] = (unsigned char)(sumAlpha / count);
            }
        }

        stbi_image_free(pixelData);
        *width = targetWidth;
        *height = targetHeight;
        return scaled;
    }

    /// @brief Decode and upload one option icon, caching the texture slot.
    /// @param hostApi Host API.
    /// @param optionIndex Option index 0-17.
    void LoadOptionIcon(const MenuHostApi *hostApi, int optionIndex)
    {
        if (optionIndex < 0 || optionIndex >= kSettingsOptionCount) {
            return;
        }

        int imageWidth = 0;
        int imageHeight = 0;
        unsigned char *pixelData = stbi_load_from_memory(kSettingsOptionIcons[optionIndex].pngData,
                                                        *kSettingsOptionIcons[optionIndex].pngSize,
                                                        &imageWidth, &imageHeight, nullptr, 4);

        if (!pixelData || imageWidth <= 0 || imageHeight <= 0) {
            if (pixelData) {
                stbi_image_free(pixelData);
            }

            return;
        }

        pixelData = DownscaleIcon(hostApi, pixelData, &imageWidth, &imageHeight);

        int textureSlot = hostApi->gfxTexAlloc();

        if (textureSlot < 0) {
            stbi_image_free(pixelData);
            return;
        }

        if (hostApi->gfxTexUploadRgba(textureSlot, pixelData, imageWidth, imageHeight) == 0) {
            hostApi->gfxTexFree(textureSlot);
            stbi_image_free(pixelData);
            return;
        }

        stbi_image_free(pixelData);
        if (hostApi->gfxIdentityBind)
            hostApi->gfxIdentityBind("settings", textureSlot);
        s_state.optionIconSlots[optionIndex] = textureSlot;
    }

    /// @brief Decode and upload every option icon.
    /// @param hostApi Host API.
    void LoadOptionIcons(const MenuHostApi *hostApi)
    {
        for (int optionIndex = 0; optionIndex < kSettingsOptionCount; optionIndex++) {
            LoadOptionIcon(hostApi, optionIndex);
        }
    }

    /// @brief Release every cached option icon texture.
    /// @param hostApi Host API.
    void DropOptionIcons(const MenuHostApi *hostApi)
    {
        for (int optionIndex = 0; optionIndex < kSettingsOptionCount; optionIndex++) {
            if (s_state.optionIconSlots[optionIndex] >= 0) {
                hostApi->gfxTexFree(s_state.optionIconSlots[optionIndex]);
                s_state.optionIconSlots[optionIndex] = -1;
            }
        }
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

    /// @brief Draw the left settings option list with icons.
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
            hostApi->touchRegisterCell(optionIndex, 110.0f + slideOffsetX, rowY - 4.0f, 500.0f, rowHeight);

            if (isSelected) {
                float accentRed = 0.0f;
                float accentGreen = 0.0f;
                float accentBlue = 0.0f;
                hostApi->themeColor("accent", &accentRed, &accentGreen, &accentBlue);
                hostApi->gfxPushPanel(110.0f + slideOffsetX, rowY - 4.0f, 500.0f, rowHeight, 8.0f,
                                      accentRed, accentGreen, accentBlue, 1.0f);
            }

            int iconSlot = s_state.optionIconSlots[optionIndex];

            if (hostApi->gfxTexValid(iconSlot)) {
                hostApi->gfxPushIcon(118.0f + slideOffsetX, rowY - 2.0f, 26.0f, 26.0f,
                                     6.0f, 6.0f, 6.0f, 6.0f, 1.0f, 1.0f, 1.0f, 0.0f, iconSlot,
                                     0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            }

            hostApi->fontDraw(kSettingsOptions[optionIndex], 152.0f + slideOffsetX, rowY + 22.0f, 27.0f,
                              isSelected ? 1.0f : (s_state.pageIndex == optionIndex ? inkRed : dimRed),
                              isSelected ? 1.0f : (s_state.pageIndex == optionIndex ? inkGreen : dimGreen),
                              isSelected ? 1.0f : (s_state.pageIndex == optionIndex ? inkBlue : dimBlue),
                              1.0f);
        }
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
            hostApi->touchRegisterCell(themeIndex, cardX - 8.0f, cardY - 8.0f, cardWidth + 16.0f,
                                       cardHeight + 16.0f);

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

    /// @brief Draw the right detail pane with the mockup header title.
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

        hostApi->fontDraw(kSettingsTitles[s_state.pageIndex], 700.0f + slideOffsetX, 250.0f, 40.0f,
                          inkRed, inkGreen, inkBlue, 1.0f);

        if (s_state.pageIndex == kSettingsThemesPage) {
            DrawThemesDetail(hostApi, slideOffsetX);
            return;
        }

        const SettingsRow *pageRows = SettingsPageRows(s_state.pageIndex);
        int rowCount = SettingsRowCount(pageRows);

        if (rowCount <= 0) {
            return;
        }

        float detailTopY = 320.0f;
        float detailRowHeight = 64.0f;
        float columnWidth = 500.0f;
        float columnGap = 40.0f;

        int columns = SettingsPageColumns(s_state.pageIndex);
        float columnX[2] = { 700.0f + slideOffsetX, 700.0f + columnWidth + columnGap + slideOffsetX };

        for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            int columnIndex = 0;
            int columnRow = rowIndex;

            if (columns > 1) {
                SettingsCellSplit(s_state.pageIndex, rowIndex, &columnIndex, &columnRow);
            }

            float detailX = columnIndex == 0 ? columnX[0] : columnX[1];
            float detailY = detailTopY + (float)columnRow * detailRowHeight;
            bool isSelected = (s_state.rowIndex == rowIndex);
            hostApi->touchRegisterCell(rowIndex, detailX - 10.0f, detailY - 6.0f, columnWidth + 18.0f,
                                       detailRowHeight + 6.0f);

            if (isSelected) {
                float accentRed = 0.0f;
                float accentGreen = 0.0f;
                float accentBlue = 0.0f;
                hostApi->themeColor("accent", &accentRed, &accentGreen, &accentBlue);
                hostApi->gfxPushPanel(detailX - 10.0f, detailY - 6.0f, columnWidth + 18.0f, detailRowHeight, 12.0f,
                                      accentRed, accentGreen, accentBlue, 0.30f);
            }

            float labelRed = isSelected ? inkRed : dimRed;
            float labelGreen = isSelected ? inkGreen : dimGreen;
            float labelBlue = isSelected ? inkBlue : dimBlue;

            hostApi->fontDraw(pageRows[rowIndex].labelText, detailX + 30.0f, detailY + 40.0f, 32.0f,
                              labelRed, labelGreen, labelBlue, 1.0f);
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

        if (pageRows[s_state.rowIndex].rowKind == kSettingsRowAction) {
            if (pageRows[s_state.rowIndex].actionId == kSettingsActionControllers) {
                hostApi->qextLaunchApplet(1);
            } else if (pageRows[s_state.rowIndex].actionId == kSettingsActionMii) {
                hostApi->qextLaunchApplet(2);
            }
        }
    }

    /// @brief Act on a registered settings cell (select, re-tap acts).
    /// @param hostApi Host API.
    /// @param cellIndex Cell index from touchRegisterCell.
    /// @return True (always consumed inside the tab).
    bool TouchAction(const MenuHostApi *hostApi, int cellIndex)
    {
        if (s_state.pageIndex < 0) {
            if (cellIndex < 0 || cellIndex >= kSettingsOptionCount) {
                return true;
            }

            if (cellIndex == s_state.rowIndex) {
                s_state.pageIndex = cellIndex;
                s_state.rowIndex = 0;
                hostApi->sfxPlayByName("click");
            } else {
                s_state.rowIndex = cellIndex;
                hostApi->sfxPlayByName("hover");
            }

            return true;
        }

        if (s_state.pageIndex == kSettingsThemesPage) {
            if (cellIndex >= 0 && cellIndex < hostApi->themeCount()) {
                if (cellIndex == s_state.rowIndex) {
                    ActivateDetailRow(hostApi);
                    hostApi->sfxPlayByName("click");
                } else {
                    s_state.rowIndex = cellIndex;
                    hostApi->sfxPlayByName("hover");
                }
            }

            return true;
        }

        const SettingsRow *pageRows = SettingsPageRows(s_state.pageIndex);
        int rowCount = SettingsRowCount(pageRows);

        if (cellIndex >= 0 && cellIndex < rowCount) {
            if (cellIndex == s_state.rowIndex) {
                ActivateDetailRow(hostApi);
                hostApi->sfxPlayByName("click");
            } else {
                s_state.rowIndex = cellIndex;
                hostApi->sfxPlayByName("hover");
            }
        }

        return true;
    }

    /// @brief Handle option list input.
    /// @param hostApi Host API.
    /// @param buttonsDown Proven pressed mask.
    void HandleOptionListInput(const MenuHostApi *hostApi, unsigned long long buttonsDown)
    {
        if (buttonsDown & (unsigned long long)HidNpadButton_AnyUp) {
            s_state.rowIndex = (s_state.rowIndex + kSettingsOptionCount - 1) % kSettingsOptionCount;
            hostApi->sfxPlayByName("hover");
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_AnyDown) {
            s_state.rowIndex = (s_state.rowIndex + 1) % kSettingsOptionCount;
            hostApi->sfxPlayByName("hover");
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

    /// @brief Handle detail page input with mockup two-column navigation.
    /// @param hostApi Host API.
    /// @param buttonsDown Proven pressed mask.
    void HandleDetailInput(const MenuHostApi *hostApi, unsigned long long buttonsDown)
    {
        int rowCount = (s_state.pageIndex == kSettingsThemesPage) ? hostApi->themeCount()
                         : SettingsRowCount(SettingsPageRows(s_state.pageIndex));

        if (rowCount <= 0) {
            rowCount = 1;
        }

        int columnCount = (s_state.pageIndex == kSettingsThemesPage) ? 2 : SettingsPageColumns(s_state.pageIndex);
        bool twoColumn = columnCount > 1 && s_state.pageIndex != kSettingsThemesPage;
        int currentColumn = 0;
        int currentRow = s_state.rowIndex;

        if (twoColumn) {
            SettingsCellSplit(s_state.pageIndex, s_state.rowIndex, &currentColumn, &currentRow);
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_AnyLeft) {
            if (twoColumn) {
                currentColumn = (currentColumn + 1) % 2;
                int columnRows = SettingsPageColumnRows(s_state.pageIndex, currentColumn);

                if (currentRow >= columnRows) {
                    currentRow = columnRows - 1;
                }

                s_state.rowIndex = SettingsCellJoin(s_state.pageIndex, currentColumn, currentRow);
                hostApi->sfxPlayByName("hover");
            } else {
                int nextRow = (s_state.rowIndex + rowCount - 1) % rowCount;

                if (nextRow != s_state.rowIndex) {
                    hostApi->sfxPlayByName("hover");
                }

                s_state.rowIndex = nextRow;
            }
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_AnyRight) {
            if (twoColumn) {
                currentColumn = (currentColumn + 1) % 2;
                int columnRows = SettingsPageColumnRows(s_state.pageIndex, currentColumn);

                if (currentRow >= columnRows) {
                    currentRow = columnRows - 1;
                }

                s_state.rowIndex = SettingsCellJoin(s_state.pageIndex, currentColumn, currentRow);
                hostApi->sfxPlayByName("hover");
            } else {
                int nextRow = (s_state.rowIndex + 1) % rowCount;

                if (nextRow != s_state.rowIndex) {
                    hostApi->sfxPlayByName("hover");
                }

                s_state.rowIndex = nextRow;
            }
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_AnyUp) {
            if (twoColumn) {
                int columnRows = SettingsPageColumnRows(s_state.pageIndex, currentColumn);
                currentRow = (currentRow + columnRows - 1) % columnRows;
                s_state.rowIndex = SettingsCellJoin(s_state.pageIndex, currentColumn, currentRow);
                hostApi->sfxPlayByName("hover");
            } else {
                int nextRow = (s_state.rowIndex + rowCount - columnCount) % rowCount;

                if (nextRow != s_state.rowIndex) {
                    hostApi->sfxPlayByName("hover");
                }

                s_state.rowIndex = nextRow;
            }
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_AnyDown) {
            if (twoColumn) {
                int columnRows = SettingsPageColumnRows(s_state.pageIndex, currentColumn);
                currentRow = (currentRow + 1) % columnRows;
                s_state.rowIndex = SettingsCellJoin(s_state.pageIndex, currentColumn, currentRow);
                hostApi->sfxPlayByName("hover");
            } else {
                int nextRow = (s_state.rowIndex + columnCount) % rowCount;

                if (nextRow != s_state.rowIndex) {
                    hostApi->sfxPlayByName("hover");
                }

                s_state.rowIndex = nextRow;
            }
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
    /// @return Clip id or NULL.
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

    /// @brief Heap profile needed by this submenu (Cap for settings).
    /// @return 1 = cap (384MB heap on enter, restored on exit).
    QLAUNCH_HOOK int SubmenuHeapProfile(void)
    {
        return 1;
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

        for (int optionIndex = 0; optionIndex < kSettingsOptionCount; optionIndex++) {
            s_state.optionIconSlots[optionIndex] = -1;
        }

        s_hostApi = hostApi;
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

    /// @brief Act on a registered settings cell (select, re-tap acts).
    /// @param cellIndex Cell index from touchRegisterCell.
    /// @return Nonzero when the tap was consumed.
    QLAUNCH_HOOK int SubmenuTouchAction(int cellIndex)
    {
        if (!s_hostApi) {
            return 0;
        }

        return TouchAction(s_hostApi, cellIndex) ? 1 : 0;
    }

    /// @brief Enter the submenu tab.
    /// @param hostApi Host API.
    static void LoadOptionIconsStep(const MenuHostApi *hostApi, int maxPerCall)
    {
        if (!hostApi || maxPerCall <= 0)
            return;
        int loaded = 0;
        for (int optionIndex = 0; optionIndex < kSettingsOptionCount && loaded < maxPerCall; optionIndex++) {
            if (s_state.optionIconSlots[optionIndex] < 0) {
                LoadOptionIcon(hostApi, optionIndex);
                loaded++;
            }
        }
    }

    QLAUNCH_HOOK void SubmenuOpen(const MenuHostApi *hostApi)
    {
        if (!hostApi)
            hostApi = s_hostApi;
        if (!hostApi)
            return;
        s_hostApi = hostApi;
        DropOptionIcons(hostApi);
        if (hostApi->gfxIdentityRegister)
            hostApi->gfxIdentityRegister("settings", 0);
        /* Icons load lazily over frames in SubmenuTick to avoid a hitch. */
    }

    /// @brief Exit the submenu tab.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuClose(const MenuHostApi *hostApi)
    {
        if (!hostApi)
            hostApi = s_hostApi;
        if (hostApi)
            DropOptionIcons(hostApi);
    }

    /// @brief Tick the submenu once per frame.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuTick(const MenuHostApi *hostApi)
    {
        if (!hostApi)
            hostApi = s_hostApi;
        if (hostApi)
            LoadOptionIconsStep(hostApi, 1);
    }

    /// @brief Shut down before unload.
    QLAUNCH_HOOK void SubmenuShutdown(void)
    {
        if (s_hostApi) {
            DropOptionIcons(s_hostApi);
        }
    }

} /* extern C */
