/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "MenuHost.hpp"
#include "SubmenuApi.hpp"
#include "Gfx.hpp"
#include "Icons.hpp"
#include "Layout.hpp"
#include "App.hpp"
#include "Sfx.hpp"
#include "Qext.hpp"
#include "Input.hpp"
#include <string.h>
#include <stdio.h>
#include <math.h>

namespace WStrip {

    static const MenuHostApi *s_hostApi = nullptr;

    namespace {

        /// @brief Scroll + highlight state for the game strip, single global.
        struct StripState {
            float stripScroll;
            float stripTarget;
            float stripMaxScroll;
            float highlightProgress;
            float markerTime;
            InputRepeat keyRepeat;
        };

        static StripState s_state = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, { 0 } };

        /// @brief Home strip phase derived from App state.
        enum class StripPhase {
            Hidden,
            Folders,
            VirtualCards,
            Games,
        };

        /// @brief Current strip phase for the Input state machine.
        /// @return Games when the strip owns input, else the sub-screen.
        StripPhase CurrentPhase()
        {
            if (s_hostApi->topFocus() || s_hostApi->appMenu() != s_hostApi->tabFindById("home")) {
                return StripPhase::Hidden;
            }

            if (s_hostApi->homeShiftBusy()) {
                return StripPhase::Hidden;
            }

            if (s_hostApi->bottomFocus()) {
                return StripPhase::Hidden;
            }

            switch (s_hostApi->homeSub()) {
                case kHomeSubFolders:
                    return StripPhase::Folders;
                case kHomeSubVgc:
                    return StripPhase::VirtualCards;
                default:
                    return StripPhase::Games;
            }
        }

        /// @brief Cached open-folder view (members resolved from qext titles).
        struct FolderViewCache {
            u64 folderId;
            u64 tids[kFolderStripGames];
            int iconSlots[kFolderStripGames];
            bool ejected[kFolderStripGames];
            char names[kFolderStripGames][kAppNameMax];
            int count;
            int syncSel;
        };

        static FolderViewCache s_view = { 0, { 0 }, { 0 }, { false }, { { 0 } }, 0, -1 };
        static char s_folderName[kAppNameMax] = { 0 };
        static const char kUnknownGame[] = "Unknown title";

        /// @brief Resolve open-folder members into the view cache.
        /// Members follow loader order so recency matches the strip.
        /// @return True when the view is valid.
        static bool ResolveMembers()
        {
            u64 folderId = s_hostApi->openFolder();

            if (folderId == 0) {
                return false;
            }

            int folderCount = qext_folder_count();

            if (folderCount < 0) {
                folderCount = 0;
            }

            int folderIndex = -1;

            for (int i = 0; i < folderCount; i++) {
                if (qext_folder_id(i) == folderId) {
                    folderIndex = i;
                    break;
                }
            }

            if (folderIndex < 0) {
                return false;
            }

            s_folderName[0] = 0;

            if (qext_folder_name(folderIndex, s_folderName, kAppNameMax) < 0 || s_folderName[0] == 0) {
                const char *folderFallback = "Folder";
                unsigned fallbackCursor = 0;

                while (folderFallback[fallbackCursor] && fallbackCursor + 1 < kAppNameMax) {
                    s_folderName[fallbackCursor] = folderFallback[fallbackCursor];
                    fallbackCursor++;
                }

                s_folderName[fallbackCursor] = 0;
            } else {
                s_folderName[kAppNameMax - 1] = 0;
            }

            int memberCount = qext_folder_game_count(folderIndex);

            if (memberCount < 0) {
                memberCount = 0;
            }

            if (memberCount > kFolderStripGames) {
                memberCount = kFolderStripGames;
            }

            u64 memberTids[kFolderStripGames];

            for (int m = 0; m < memberCount; m++) {
                memberTids[m] = qext_folder_game(folderIndex, m);
            }

            int titleCount = qext_title_count();

            if (titleCount < 0) {
                titleCount = 0;
            }

            if (titleCount > kAppMaxTitles) {
                titleCount = kAppMaxTitles;
            }

            s_view.folderId = folderId;
            s_view.count = 0;

            for (int titleIndex = 0; titleIndex < titleCount && s_view.count < kFolderStripGames; titleIndex++) {
                u64 titleTid = qext_title_id(titleIndex);
                bool isMember = false;

                for (int m = 0; m < memberCount; m++) {
                    if (memberTids[m] == titleTid) {
                        isMember = true;
                        break;
                    }
                }

                if (!isMember) {
                    continue;
                }

                int slot = s_view.count++;
                s_view.tids[slot] = titleTid;
                s_view.ejected[slot] = qext_title_ejected(titleIndex) > 0;
                s_view.iconSlots[slot] = -1;
                s_view.names[slot][0] = 0;

                int nameLen = qext_title_name(titleIndex, s_view.names[slot], kAppNameMax);

                if (nameLen < 0 || s_view.names[slot][0] == 0) {
                    unsigned k = 0;

                    while (kUnknownGame[k] && k + 1 < kAppNameMax) {
                        s_view.names[slot][k] = kUnknownGame[k];
                        k++;
                    }

                    s_view.names[slot][k] = 0;
                } else {
                    s_view.names[slot][kAppNameMax - 1] = 0;
                }
            }

            /* Orphaned members (prune races) trail with fallback names. */
            for (int m = 0; m < memberCount && s_view.count < kFolderStripGames; m++) {
                bool known = false;

                for (int s = 0; s < s_view.count; s++) {
                    if (s_view.tids[s] == memberTids[m]) {
                        known = true;
                        break;
                    }
                }

                if (known || memberTids[m] == 0) {
                    continue;
                }

                int slot = s_view.count++;
                s_view.tids[slot] = memberTids[m];
                s_view.ejected[slot] = false;
                s_view.iconSlots[slot] = -1;

                unsigned k = 0;

                while (kUnknownGame[k] && k + 1 < kAppNameMax) {
                    s_view.names[slot][k] = kUnknownGame[k];
                    k++;
                }

                s_view.names[slot][k] = 0;
            }

            s_view.syncSel = -1;
            return true;
        }

        /// @brief Sync member icons, recentering the budget on selection.
        static void SyncMemberIcons()
        {
            u64 syncIds[kFolderStripGames];
            int syncSlots[kFolderStripGames];

            for (int m = 0; m < s_view.count; m++) {
                syncIds[m] = s_view.tids[m];
                syncSlots[m] = -1;
            }

            int sel = s_hostApi->folderSel();

            if (sel < 0 || sel >= s_view.count) {
                sel = 0;
            }

            s_hostApi->iconsSyncTitle(syncIds, syncSlots, s_view.count, sel);

            for (int m = 0; m < s_view.count; m++) {
                s_view.iconSlots[m] = syncSlots[m];
            }

            s_view.syncSel = s_hostApi->folderSel();
        }

        /// @brief Validate the folder view, rebuilding the cache as needed.
        /// Closes the view when its folder is gone.
        /// @return True when a valid view is showing.
        static bool ValidateFolderView()
        {
            if (s_hostApi->openFolder() == 0) {
                return false;
            }

            if (s_hostApi->openFolder() != s_view.folderId) {
                if (!ResolveMembers()) {
                    s_hostApi->closeFolderView();
                    return false;
                }

                SyncMemberIcons();
                return s_view.count > 0;
            }

            int memberCount = 0;

            {
                int folderCount = qext_folder_count();

                if (folderCount > 0) {
                    for (int i = 0; i < folderCount; i++) {
                        if (qext_folder_id(i) == s_hostApi->openFolder()) {
                            memberCount = qext_folder_game_count(i);

                            if (memberCount < 0) {
                                memberCount = 0;
                            }

                            break;
                        }
                    }
                }
            }

            if (memberCount != s_view.count) {
                if (!ResolveMembers()) {
                    s_hostApi->closeFolderView();
                    return false;
                }
            }

            if (s_view.count <= 0) {
                s_hostApi->closeFolderView();
                return false;
            }

            if (s_hostApi->folderSel() != s_view.syncSel) {
                SyncMemberIcons();
            }

            return true;
        }

    } // namespace

    constexpr int kStripPreviewMax = 5;

    /// @brief Folder tile palette entry.
    struct FolderColorEntry {
        const char *name;
        float red;
        float green;
        float blue;
    };

    constexpr FolderColorEntry kFolderColorEntries[] = {
        { "Default", 0.0f, 0.0f, 0.0f },
        { "Red", 0.85f, 0.30f, 0.30f },
        { "Orange", 0.90f, 0.55f, 0.20f },
        { "Yellow", 0.85f, 0.72f, 0.25f },
        { "Green", 0.30f, 0.70f, 0.35f },
        { "Teal", 0.25f, 0.65f, 0.65f },
        { "Blue", 0.30f, 0.50f, 0.90f },
        { "Purple", 0.55f, 0.40f, 0.85f },
        { "Pink", 0.90f, 0.45f, 0.65f },
        { "Grey", 0.50f, 0.50f, 0.55f },
    };

    /// @brief Resolve a folder palette entry (theme override or builtin).
    /// Index 0 always follows the theme panel.
    /// @param colorIndex Palette index.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    static void FolderPaletteColor(int colorIndex, float *redOut, float *greenOut, float *blueOut)
    {
        if (colorIndex > 0 && colorIndex < kFolderColorCount) {
            char keyText[16];
            snprintf(keyText, sizeof(keyText), "folder%d", colorIndex);

            if (s_hostApi->themeTryColor(keyText, redOut, greenOut, blueOut)) {
                return;
            }

            *redOut = kFolderColorEntries[colorIndex].red;
            *greenOut = kFolderColorEntries[colorIndex].green;
            *blueOut = kFolderColorEntries[colorIndex].blue;
            return;
        }

        s_hostApi->themeColor("panel", redOut, greenOut, blueOut);
    }

    /// @brief Draw a folder tile (panel plus folder art).
    /// @param tileX X position.
    /// @param tileY Y position.
    /// @param tileSize Tile edge length.
    /// @param cornerRadius Top corner radius.
    /// @param bottomRadius Bottom corner radius (square under the grow band).
    /// @param opacity Opacity.
    /// @param colorIndex Palette index (0 follows the theme panel).
    /// @param previewSlots Mini icon slots, or null for icon only.
    /// @param previewCount Mini icon count.
    static void DrawFolderArt(float tileX, float tileY, float tileSize, float cornerRadius, float bottomRadius, float opacity,
                              int colorIndex, const int *previewSlots, int previewCount)
    {
        float panelRed = 0.0f;
        float panelGreen = 0.0f;
        float panelBlue = 0.0f;

        FolderPaletteColor(colorIndex, &panelRed, &panelGreen, &panelBlue);

        s_hostApi->gfxPushPanelCorners(tileX, tileY, tileSize, tileSize, cornerRadius, cornerRadius, bottomRadius, bottomRadius,
                                panelRed, panelGreen, panelBlue, opacity);

        float glyphRed = 1.0f;
        float glyphGreen = 1.0f;
        float glyphBlue = 1.0f;

        if (colorIndex > 0 && colorIndex < kFolderColorCount) {
            glyphRed = panelRed * 0.45f;
            glyphGreen = panelGreen * 0.45f;
            glyphBlue = panelBlue * 0.45f;
        }

        int folderSlot = s_hostApi->iconsHudSlot(kHudFolder);

        if (s_hostApi->gfxTexValid(folderSlot)) {
            float iconSize = tileSize * 0.5f;
            float iconOffsetX = (tileSize - iconSize) * 0.5f;
            s_hostApi->gfxPushIcon(tileX + iconOffsetX, tileY + tileSize * 0.07f, iconSize, iconSize, cornerRadius, cornerRadius,
                           bottomRadius, bottomRadius, glyphRed, glyphGreen, glyphBlue, 0.0f, folderSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, opacity);
        }

        if (!previewSlots || previewCount <= 0) {
            return;
        }

        int shownCount = previewCount > kStripPreviewMax ? kStripPreviewMax : previewCount;
        float miniSize = tileSize * 0.16f;
        float miniGap = tileSize * 0.02f;
        float miniTotal = (float)shownCount * miniSize + (float)(shownCount - 1) * miniGap;
        float miniX = tileX + (tileSize - miniTotal) * 0.5f;
        float miniY = tileY + tileSize - miniSize - tileSize * 0.055f;
        float miniCorner = miniSize * 0.22f;
        float menuBarRed = 0.0f;
        float menuBarGreen = 0.0f;
        float menuBarBlue = 0.0f;
        s_hostApi->themeColor("menubar", &menuBarRed, &menuBarGreen, &menuBarBlue);

        for (int miniIndex = 0; miniIndex < shownCount; miniIndex++) {
            int miniSlot = previewSlots[miniIndex];

            if (s_hostApi->gfxTexValid(miniSlot)) {
                s_hostApi->gfxPushIcon(miniX, miniY, miniSize, miniSize, miniCorner, miniCorner, miniCorner, miniCorner,
                               1.0f, 1.0f, 1.0f, 0.0f, miniSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, opacity);
            } else {
                s_hostApi->gfxPushPanel(miniX, miniY, miniSize, miniSize, miniCorner,
                               menuBarRed, menuBarGreen, menuBarBlue, opacity);
            }

            miniX += miniSize + miniGap;
        }
    }

    /// @brief Cached strip preview: first games of one folder.
    struct StripPreview {
        u64 folderId;
        u64 tids[kStripPreviewMax];
        int slots[kStripPreviewMax];
        int count;
        int memberCount;
    };

    static StripPreview s_previews[kFolderStripMax];

    /// @brief Find a preview entry by folder id, claiming a slot if needed.
    /// @param folderId Folder id.
    /// @return Entry (possibly foreign, caller must validate).
    static StripPreview *FindPreview(u64 folderId)
    {
        for (int i = 0; i < kFolderStripMax; i++) {
            if (s_previews[i].folderId == folderId) {
                return &s_previews[i];
            }
        }

        for (int i = 0; i < kFolderStripMax; i++) {
            if (s_previews[i].folderId == 0) {
                return &s_previews[i];
            }
        }

        return &s_previews[0];
    }

    /// @brief Sync one preview entry's icons.
    /// @param preview Entry with fresh tids.
    static void SyncPreviewIcons(StripPreview *preview)
    {
        u64 syncIds[kStripPreviewMax];
        int syncSlots[kStripPreviewMax];

        for (int i = 0; i < preview->count; i++) {
            syncIds[i] = preview->tids[i];
            syncSlots[i] = -1;
        }

        s_hostApi->iconsSyncTitle(syncIds, syncSlots, preview->count, s_hostApi->appSelection());

        for (int i = 0; i < preview->count; i++) {
            preview->slots[i] = syncSlots[i];
        }
    }

    /// @brief Resolve a folder's strip preview, refreshing on change.
    /// @param folderId Folder id.
    /// @return Entry with icon slots, or null when empty.
    static StripPreview *ResolvePreview(u64 folderId)
    {
        if (folderId == 0) {
            return nullptr;
        }

        StripPreview *preview = FindPreview(folderId);
        bool needsBuild = (preview->folderId != folderId);

        int folderCount = qext_folder_count();

        if (folderCount < 0) {
            folderCount = 0;
        }

        int folderIndex = -1;

        for (int i = 0; i < folderCount; i++) {
            if (qext_folder_id(i) == folderId) {
                folderIndex = i;
                break;
            }
        }

        if (folderIndex < 0) {
            preview->folderId = 0;
            preview->count = 0;
            preview->memberCount = 0;
            return nullptr;
        }

        int memberCount = qext_folder_game_count(folderIndex);

        if (memberCount < 0) {
            memberCount = 0;
        }

        if (memberCount != preview->memberCount) {
            needsBuild = true;
        }

        if (needsBuild) {
            preview->folderId = folderId;
            preview->memberCount = memberCount;
            preview->count = 0;

            for (int m = 0; m < memberCount && preview->count < kStripPreviewMax; m++) {
                u64 memberTid = qext_folder_game(folderIndex, m);

                if (memberTid == 0) {
                    continue;
                }

                preview->tids[preview->count] = memberTid;
                preview->slots[preview->count] = -1;
                preview->count++;
            }

            SyncPreviewIcons(preview);
        } else {
            bool healed = false;

            for (int i = 0; i < preview->count; i++) {
                if (!s_hostApi->gfxTexValid(preview->slots[i])) {
                    healed = true;
                    break;
                }
            }

            if (healed) {
                SyncPreviewIcons(preview);
            }
        }

        return preview->count > 0 ? preview : nullptr;
    }

    /// @brief Forward declarations for the options popup (defined below).
    static void CreateFolderFlow(u64 titleId, u64 removeFromFolder);
    static void RenameFolderFlow(u64 folderId, const char *currentName);

    /// @brief Draw a fallback tile when no icon is available.
    /// @param row Row to represent.
    /// @param tileX X position.
    /// @param tileY Y position.
    /// @param tileWidth Width.
    /// @param tileHeight Height.
    /// @param cornerRadius Corner radius.
    /// @param opacity Opacity.
    static void TileFallback(const AppRow &row, float tileX, float tileY, float tileWidth, float tileHeight, float cornerRadius, float opacity)
    {
        float tileRed = 0.5f;
        float tileGreen = 0.5f;
        float tileBlue = 0.55f;
        unsigned fallbackGlyph = 0;

        if (row.isFolder) {
            DrawFolderArt(tileX, tileY, tileWidth, cornerRadius, cornerRadius, opacity, row.folderColor, nullptr, 0);
            return;
        }

        if (row.isGame) {
            s_hostApi->themeColor("menubar", &tileRed, &tileGreen, &tileBlue);
        } else if (row.applet == 0) {
            s_hostApi->themeColor("tile_album", &tileRed, &tileGreen, &tileBlue);
            fallbackGlyph = GLYPH_ALBUM;
        } else {
            s_hostApi->themeColor("tile_ctrl", &tileRed, &tileGreen, &tileBlue);
            fallbackGlyph = GLYPH_JOYCON;
        }

        float tileSize = tileWidth < tileHeight ? tileWidth : tileHeight;
        s_hostApi->gfxPushPanel(tileX, tileY, tileWidth, tileHeight, cornerRadius, tileRed, tileGreen, tileBlue, opacity);

        if (fallbackGlyph) {
            char glyphText[4] = { 0 };
            GlyphUTF8(fallbackGlyph, glyphText);
            float glyphSize = tileSize * 0.45f;
            float glyphWidth = s_hostApi->fontMeasure(glyphText, glyphSize);

            s_hostApi->fontDraw(glyphText, tileX + (tileSize - glyphWidth) * 0.5f, tileY + tileSize * 0.5f + glyphSize * 0.35f,
                       glyphSize, 1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            const char *nameCursor = row.name;
            uint32_t codepoint = s_hostApi->fontNext(&nameCursor);

            if (codepoint < 32u || codepoint > 255u) {
                codepoint = 63u;
            }

            char initialText[8] = { 0 };

            if (codepoint < 0x80) {
                initialText[0] = (char)codepoint;
            } else if (codepoint < 0x800) {
                initialText[0] = (char)(0xC0 | (codepoint >> 6));
                initialText[1] = (char)(0x80 | (codepoint & 0x3F));
            } else {
                initialText[0] = 63;
            }

            float letterSize = tileSize * 0.42f;
            float letterWidth = s_hostApi->fontMeasure(initialText, letterSize);

            s_hostApi->fontDraw(initialText, tileX + (tileSize - letterWidth) * 0.5f,
                       tileY + tileSize * 0.5f + letterSize * 0.35f, letterSize, 1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    /// @brief Draw the animated "new" marker above the selected tile.
    /// @param centerX Center X.
    /// @param bandTop Top of the growth band.
    /// @param bandHeight Height of the growth band.
    /// @param highlightProgress Highlight progress.
    static void DrawMarker(float centerX, float bandTop, float bandHeight, float highlightProgress)
    {
        (void)highlightProgress;
        s_state.markerTime += 1.0f / 60.0f;

        if (s_state.markerTime >= 4.0f) {
            s_state.markerTime -= 4.0f;
        }

        int frameIndex = (int)(s_state.markerTime * 12.0f);

        if (frameIndex < 0) {
            frameIndex = 0;
        }

        if (frameIndex > 47) {
            frameIndex = 47;
        }

        int markerSlot = s_hostApi->iconsHudSlot(kHudMarkerStrip);

        if (markerSlot <= 0) {
            return;
        }

        float sheetX = (float)((frameIndex % 14) * 113);
        float sheetY = (float)((frameIndex / 14) * 51);
        float uvLeft = sheetX / 1582.0f;
        float uvTop = sheetY / 204.0f;
        float uvRight = (sheetX + 113.0f) / 1582.0f;
        float uvBottom = (sheetY + 51.0f) / 204.0f;
        float markerWidth = 100.0f;
        float markerHeight = 45.0f;
        float markerX = centerX - markerWidth * 0.5f;
        float markerY = bandTop + (bandHeight - markerHeight) * 0.5f;
        s_hostApi->gfxPushIcon(markerX, markerY, markerWidth, markerHeight, 6.0f, 6.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, markerSlot, uvLeft, uvTop, uvRight, uvBottom, 1.0f, 1.0f);
    }

    /// @brief Whether a title id is the currently running game.
    /// @param tid Title id.
    /// @return True when the game runs now.
    bool IsRunningTid(u64 tid)
    {
        return tid != 0 && s_hostApi->qextGameRunning() && s_hostApi->qextSuspendedTitle() == tid;
    }

    /// @brief Whether a strip row is the running game.
    /// @param row Strip row.
    /// @return True for the running, inserted game.
    bool IsRunningRow(const AppRow &row)
    {
        return row.isGame && !row.ejected && IsRunningTid(row.tid);
    }

    /// @brief Hop lift for the running tile (one up-down cycle per 0.5s).
    /// @return Pixels to lift the tile.
    float BounceLift()
    {
        float cycle = s_hostApi->appTime() / 0.5f;
        float phase = cycle - (float)(int)cycle;
        return 12.0f * (0.5f - 0.5f * cosf(phase * 6.2831853f));
    }

    /// @brief Whether a folder contains the currently running game.
    /// @param folderId Folder id.
    /// @return True when a member matches the suspended title.
    bool FolderHasRunning(u64 folderId)
    {
        u64 runningTid = s_hostApi->qextGameRunning() ? s_hostApi->qextSuspendedTitle() : 0;

        if (runningTid == 0 || folderId == 0) {
            return false;
        }

        int folderCount = qext_folder_count();

        if (folderCount < 0) {
            return false;
        }

        for (int folderIndex = 0; folderIndex < folderCount; folderIndex++) {
            if (qext_folder_id(folderIndex) != folderId) {
                continue;
            }

            int memberCount = qext_folder_game_count(folderIndex);

            for (int memberIndex = 0; memberIndex < memberCount; memberIndex++) {
                if (qext_folder_game(folderIndex, memberIndex) == runningTid) {
                    return true;
                }
            }

            return false;
        }

        return false;
    }

    /// @brief Whether a strip tile demands attention (running or holds it).
    /// @param row Strip row.
    /// @return True when the tile bounces with a steady ring.
    bool IsAttractedRow(const AppRow &row)
    {
        if (IsRunningRow(row)) {
            return true;
        }

        return row.isFolder && FolderHasRunning(row.folderId);
    }

    /// @brief Draw a title wrapped to two lines under the selected tile.
    /// @param titleName Title name.
    /// @param centerX Center X.
    /// @param topY Top Y of the label block.
    static void DrawWrappedName(const char *titleName, float centerX, float topY)
    {
        char wordBuffer[128];
        unsigned bufferLength = 0;

        while (titleName[bufferLength] != 0 && bufferLength + 1 < sizeof(wordBuffer)) {
            wordBuffer[bufferLength] = titleName[bufferLength];
            bufferLength++;
        }

        wordBuffer[bufferLength] = 0;

        const char *wordList[16];
        int wordCount = 0;

        for (unsigned bufferIndex = 0; bufferIndex < bufferLength && wordCount < 16;) {
            while (bufferIndex < bufferLength && wordBuffer[bufferIndex] == 32) {
                bufferIndex++;
            }

            if (bufferIndex >= bufferLength) {
                break;
            }

            wordList[wordCount++] = &wordBuffer[bufferIndex];

            while (bufferIndex < bufferLength && wordBuffer[bufferIndex] != 32) {
                bufferIndex++;
            }

            if (bufferIndex < bufferLength) {
                wordBuffer[bufferIndex++] = 0;
            }
        }

        char wrappedLines[2][128];
        int lineCount = 0;
        int wordIndex = 0;

        while (wordIndex < wordCount && lineCount < 2) {
            wrappedLines[lineCount][0] = 0;
            unsigned lineLength = 0;

            while (wordIndex < wordCount) {
                const char *wordText = wordList[wordIndex];
                unsigned wordLength = 0;

                while (wordText[wordLength] != 0) {
                    wordLength++;
                }

                unsigned neededLength = lineLength + (lineLength ? 1 : 0) + wordLength;

                if (lineLength > 0 && neededLength >= 127) {
                    break;
                }

                if (lineLength > 0) {
                    wrappedLines[lineCount][lineLength++] = 32;
                }

                for (unsigned letterIndex = 0; letterIndex < wordLength && lineLength + 1 < 127; letterIndex++) {
                    wrappedLines[lineCount][lineLength++] = wordText[letterIndex];
                }

                wrappedLines[lineCount][lineLength] = 0;
                wordIndex++;

                if (lineCount == 0 && wordIndex < wordCount) {
                    char trialText[128];
                    unsigned trialLength = 0;

                    while (wrappedLines[lineCount][trialLength] != 0 && trialLength + 1 < sizeof(trialText)) {
                        trialText[trialLength] = wrappedLines[lineCount][trialLength];
                        trialLength++;
                    }

                    trialText[trialLength++] = 32;
                    unsigned nextLength = 0;

                    while (wordList[wordIndex][nextLength] != 0 && trialLength + 1 < sizeof(trialText)) {
                        trialText[trialLength++] = wordList[wordIndex][nextLength++];
                    }

                    trialText[trialLength] = 0;

                    if (s_hostApi->fontMeasure(trialText, 30.0f) > 460.0f) {
                        break;
                    }
                }
            }

            if (wrappedLines[lineCount][0] != 0) {
                lineCount++;
            } else {
                break;
            }
        }

        while (wordIndex < wordCount && lineCount > 0) {
            unsigned lineLength = 0;

            while (wrappedLines[lineCount - 1][lineLength] != 0) {
                lineLength++;
            }

            const char *wordText = wordList[wordIndex++];

            if (lineLength > 0 && lineLength + 1 < 127) {
                wrappedLines[lineCount - 1][lineLength++] = 32;
            }

            while (wordText[0] != 0 && lineLength + 1 < 127) {
                wrappedLines[lineCount - 1][lineLength++] = wordText[0];
                wordText++;
            }

            wrappedLines[lineCount - 1][lineLength] = 0;
        }

        float labelRed = 0.043f;
        float labelGreen = 0.337f;
        float labelBlue = 0.804f;
        s_hostApi->themeTryColor("gamelabel", &labelRed, &labelGreen, &labelBlue);

        for (int lineIndex = 0; lineIndex < lineCount; lineIndex++) {
            s_hostApi->fontCentered(wrappedLines[lineIndex], centerX, topY + (float)lineIndex * 36.0f, 30.0f, 460.0f, labelRed, labelGreen, labelBlue, 1.0f);
        }
    }

    /// @brief Draw an expanded (popped-up) tile with ring, art, and marker.
    /// The art rises by the growth height while the bottom edge stays put.
    /// @param row Strip row.
    /// @param tileX Tile left X.
    /// @param tileTop Tile top Y at rest (bounce-lifted for running games).
    /// @param outerSize Tile outer size.
    /// @param cornerRadius Corner radius.
    /// @param tileAlpha Tile opacity.
    /// @param growPixels Growth height of the popup panel.
    void DrawExpandedTile(const AppRow &row, float tileX, float tileTop, float outerSize,
                          float cornerRadius, float tileAlpha, float growPixels)
    {
        float bottomRadius = row.ejected ? cornerRadius : 0.0f;
        float artTop = tileTop - growPixels;
        float growWidth = outerSize;
        float growHeight = outerSize + growPixels;
        s_hostApi->gfxPushSelectRing(tileX - 9.0f, artTop - 9.0f, growWidth + 18.0f, growHeight + 18.0f, cornerRadius + 9.0f, 12.0f);

        if (row.isFolder) {
            StripPreview *preview = ResolvePreview(row.folderId);
            DrawFolderArt(tileX, artTop, outerSize, cornerRadius, bottomRadius, tileAlpha, row.folderColor,
                          preview ? preview->slots : nullptr, preview ? preview->count : 0);
        } else if (s_hostApi->gfxTexValid(row.iconSlot)) {
            unsigned queuedQuads = s_hostApi->gfxPushIcon(tileX, artTop, outerSize, outerSize, cornerRadius, cornerRadius, bottomRadius, bottomRadius, 1.0f, 1.0f, 1.0f, 0.0f, row.iconSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, tileAlpha);

            if (queuedQuads >= (unsigned)(kGfxIconQueueMax / 6)) {
                TileFallback(row, tileX, artTop, outerSize, outerSize, cornerRadius, tileAlpha);
            }
        } else {
            TileFallback(row, tileX, artTop, outerSize, outerSize, cornerRadius, tileAlpha);
        }

        if (growPixels > 0.5f) {
            s_hostApi->gfxPushPanelCorners(tileX, artTop + outerSize, growWidth, growPixels, 0.0f, 0.0f, cornerRadius, cornerRadius,
                                   1.0f, 1.0f, 1.0f, 1.0f);
            DrawMarker(tileX + outerSize * 0.5f, artTop + outerSize, growPixels, 1.0f);
        }
    }

    /// @brief Draw the home game strip from a layout node.
    /// @param layoutNode Gamestrip node carrying tile geometry.
    void Draw(const LayoutNode &layoutNode)
    {
        s_hostApi->gfxPushLayer((int)Gfx::GfxLayer::GameStrip);
        float cornerRadius = 24.0f;
        float frameWidth = 6.0f;
        float tileSize = layoutNode.tile > 0.0f ? layoutNode.tile : 384.0f;
        float tileGap = layoutNode.gap > 0.0f ? layoutNode.gap : 50.0f;
        float tilePitch = tileSize + tileGap;
        float slideOffsetX = s_hostApi->layoutShiftX();
        float shiftOffsetY = s_hostApi->homeShiftY();

        int rowCount = s_hostApi->rowCount();
        int selectedIndex = s_hostApi->appSelection();

        /* Focus */
        bool isGamesActive = s_hostApi->appMenu() == s_hostApi->tabFindById("home") && !s_hostApi->topFocus() &&
                           !s_hostApi->bottomFocus() && s_hostApi->homeSub() == kHomeSubGames;
        float outerSize = tileSize + frameWidth * 2.0f;

        /* Game label 30px accent, 461px wide. */
        if (layoutNode.label && isGamesActive && selectedIndex >= 0 && selectedIndex < rowCount) {
            float labelProgress = s_state.highlightProgress < 0.0f ? 0.0f : (s_state.highlightProgress > 1.0f ? 1.0f : s_state.highlightProgress);
            float titleCenterX = layoutNode.x + slideOffsetX + (float)selectedIndex * tilePitch - s_state.stripScroll + outerSize * 0.5f;
            DrawWrappedName(s_hostApi->rowAt(selectedIndex)->name, titleCenterX,
                            layoutNode.y + shiftOffsetY + outerSize + 20.0f * labelProgress + 45.0f);
        }

        float visibleWidth = 1920.0f - layoutNode.x;
        s_state.stripMaxScroll = (float)(rowCount - 1) * tilePitch + outerSize + layoutNode.x * 2.0f - 1920.0f;

        if (s_state.stripMaxScroll < 0.0f) {
            s_state.stripMaxScroll = 0.0f;
        }

        if (s_state.stripTarget < 0.0f) {
            s_state.stripTarget = 0.0f;
        }

        if (s_state.stripTarget > s_state.stripMaxScroll) {
            s_state.stripTarget = s_state.stripMaxScroll;
        }

        if (rowCount > 0 && selectedIndex >= 0 && selectedIndex < rowCount) {
            float selectedLeft = (float)selectedIndex * tilePitch;
            float selectedRight = selectedLeft + outerSize;

            if (selectedRight - s_state.stripTarget > visibleWidth - layoutNode.x) {
                s_state.stripTarget = selectedRight - (visibleWidth - layoutNode.x);
            }

            if (selectedLeft - s_state.stripTarget < 0.0f) {
                s_state.stripTarget = selectedLeft;
            }
        }

        s_state.stripScroll += (s_state.stripTarget - s_state.stripScroll) * 0.25f;

        if (s_state.stripScroll < 0.0f) {
            s_state.stripScroll = 0.0f;
        }

        float slideDistance = slideOffsetX < 0.0f ? -slideOffsetX : slideOffsetX;
        int firstTile = (int)((s_state.stripScroll - slideDistance) / tilePitch) - 2;

        if (firstTile < 0) {
            firstTile = 0;
        }

        int lastTile = (int)((s_state.stripScroll + 1920.0f + slideDistance) / tilePitch) + 3;

        if (lastTile > rowCount) {
            lastTile = rowCount;
        }

        if (s_state.highlightProgress < 1.0f) {
            s_state.highlightProgress += 1.0f / 7.0f;
        }

        if (s_state.highlightProgress > 1.0f) {
            s_state.highlightProgress = 1.0f;
        }

        /* Gamecard animation stuff */
        int selectedTile = (isGamesActive && selectedIndex >= firstTile && selectedIndex < lastTile) ? selectedIndex : -1;

        for (int tileIndex = firstTile; tileIndex < lastTile; tileIndex++) {
            if (tileIndex == selectedTile) {
                continue;
            }

            float tileCenterX = layoutNode.x + slideOffsetX + (float)tileIndex * tilePitch - s_state.stripScroll + outerSize * 0.5f;
            const AppRow &row = *s_hostApi->rowAt(tileIndex);

            if (IsAttractedRow(row)) {
                continue;
            }

            float tileAlpha = row.ejected ? 0.50f : 1.0f;
            float tileX = tileCenterX - outerSize * 0.5f;
            float tileY = layoutNode.y + shiftOffsetY;

            if (tileX + outerSize < -50.0f || tileX > 1970.0f) {
                continue;
            }

            if (row.isFolder) {
                StripPreview *preview = ResolvePreview(row.folderId);
                DrawFolderArt(tileX, tileY, outerSize, cornerRadius, cornerRadius, tileAlpha, row.folderColor,
                              preview ? preview->slots : nullptr, preview ? preview->count : 0);
            } else if (s_hostApi->gfxTexValid(row.iconSlot)) {
                unsigned queuedQuads = s_hostApi->gfxPushIcon(tileX, tileY, outerSize, outerSize, cornerRadius, cornerRadius, cornerRadius, cornerRadius, 1.0f, 1.0f, 1.0f, 0.0f, row.iconSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, tileAlpha);

                if (queuedQuads >= (unsigned)(kGfxIconQueueMax / 6)) {
                    TileFallback(row, tileX, tileY, outerSize, outerSize, cornerRadius, tileAlpha);
                }
            } else {
                TileFallback(row, tileX, tileY, outerSize, outerSize, cornerRadius, tileAlpha);
            }
        }

        if (selectedTile >= 0) {
            float tileCenterX = layoutNode.x + slideOffsetX + (float)selectedTile * tilePitch - s_state.stripScroll + outerSize * 0.5f;
            const AppRow &row = *s_hostApi->rowAt(selectedTile);
            float tileAlpha = row.ejected ? 0.50f : 1.0f;
            float tileX = tileCenterX - outerSize * 0.5f;
            float tileY = layoutNode.y + shiftOffsetY;
            float highlightProgress = row.ejected ? 0.0f : (s_state.highlightProgress < 0.0f ? 0.0f : (s_state.highlightProgress > 1.0f ? 1.0f : s_state.highlightProgress));

            /* More gamecard stuff. */
            float growPixels = 84.0f * highlightProgress;
            float bounceLift = IsAttractedRow(row) ? BounceLift() : 0.0f;
            DrawExpandedTile(row, tileX, tileY - bounceLift, outerSize, cornerRadius, tileAlpha, growPixels);
        }

        /* Running games (and folders holding one) keep the flap out and hop. */
        for (int tileIndex = firstTile; tileIndex < lastTile; tileIndex++) {
            if (tileIndex == selectedTile) {
                continue;
            }

            const AppRow &row = *s_hostApi->rowAt(tileIndex);

            if (!IsAttractedRow(row)) {
                continue;
            }

            float tileCenterX = layoutNode.x + slideOffsetX + (float)tileIndex * tilePitch - s_state.stripScroll + outerSize * 0.5f;
            float tileX = tileCenterX - outerSize * 0.5f;
            float tileY = layoutNode.y + shiftOffsetY;

            if (tileX + outerSize < -50.0f || tileX > 1970.0f) {
                continue;
            }

            DrawExpandedTile(row, tileX, tileY - BounceLift(), outerSize, cornerRadius, 1.0f, 84.0f);
        }

        s_hostApi->gfxPopLayer();
    }

    /// @brief Draw folders/VGC sub-screens under the strip.
    void DrawSubs()
    {
        s_hostApi->gfxPushLayer((int)Gfx::GfxLayer::GameStrip);
        float slideOffsetX = s_hostApi->layoutShiftX();
        float shiftOffsetY = s_hostApi->homeShiftY();

        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        s_hostApi->themeColor("ink", &inkRed, &inkGreen, &inkBlue);

        float dimRed = 0.0f;
        float dimGreen = 0.0f;
        float dimBlue = 0.0f;
        s_hostApi->themeColor("dim", &dimRed, &dimGreen, &dimBlue);

        float menuBarRed = 0.0f;
        float menuBarGreen = 0.0f;
        float menuBarBlue = 0.0f;
        s_hostApi->themeColor("menubar", &menuBarRed, &menuBarGreen, &menuBarBlue);

        /* Open folder view: member games of the selected folder. */
        {
            float foldersY = 293.0f + 830.0f + shiftOffsetY;

            if (foldersY > -400.0f && foldersY < 1100.0f && ValidateFolderView() && s_view.count > 0) {
                s_hostApi->fontCentered(s_folderName, 960.0f + slideOffsetX, foldersY - 28.0f, 44.0f,
                                   900.0f, inkRed, inkGreen, inkBlue, 1.0f);

                int folderSel = s_hostApi->folderSel();

                if (folderSel < 0) {
                    folderSel = 0;
                }

                if (folderSel >= kFolderStripGames) {
                    folderSel = kFolderStripGames - 1;
                }

                float cardWidth = 180.0f;
                float cardGap = 33.0f;
                float cardPitch = cardWidth + cardGap;
                int columnCount = 8;
                float gridStartX = 960.0f - (columnCount * cardPitch - cardGap) * 0.5f + slideOffsetX;
                float gridStartY = foldersY + 12.0f;

                for (int memberIndex = 0; memberIndex < s_view.count; memberIndex++) {
                    int gridColumn = memberIndex % columnCount;
                    int gridRow = memberIndex / columnCount;
                    float cardX = gridStartX + gridColumn * cardPitch;
                    float cardY = gridStartY + gridRow * (cardWidth + 33.0f);
                    float cardAlpha = s_view.ejected[memberIndex] ? 0.50f : 1.0f;
                    bool isSelected = (memberIndex == folderSel);
                    bool isRunning = IsRunningTid(s_view.tids[memberIndex]);
                    float cardDrawY = isRunning ? cardY - BounceLift() : cardY;

                    if (isSelected || isRunning) {
                        s_hostApi->gfxPushSelectRing(cardX - 8.0f, cardDrawY - 8.0f, cardWidth + 16.0f, cardWidth + 16.0f, 20.0f, 8.0f);
                    }

                    if (s_hostApi->gfxTexValid(s_view.iconSlots[memberIndex])) {
                        unsigned queuedQuads = s_hostApi->gfxPushIcon(cardX, cardDrawY, cardWidth, cardWidth, 12.0f, 12.0f, 12.0f, 12.0f, 1.0f, 1.0f, 1.0f, 0.0f, s_view.iconSlots[memberIndex], 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, cardAlpha);

                        if (queuedQuads >= (unsigned)(kGfxIconQueueMax / 6)) {
                            AppRow gameRow = {};
                            gameRow.isGame = true;
                            gameRow.tid = s_view.tids[memberIndex];
                            unsigned nameCursor = 0;

                            while (nameCursor + 1 < kAppNameMax && s_view.names[memberIndex][nameCursor]) {
                                gameRow.name[nameCursor] = s_view.names[memberIndex][nameCursor];
                                nameCursor++;
                            }

                            gameRow.name[nameCursor] = 0;
                            TileFallback(gameRow, cardX, cardDrawY, cardWidth, cardWidth, 12.0f, cardAlpha);
                        }
                    } else {
                        AppRow gameRow = {};
                        gameRow.isGame = true;
                        gameRow.tid = s_view.tids[memberIndex];
                        unsigned nameCursor = 0;

                        while (nameCursor + 1 < kAppNameMax && s_view.names[memberIndex][nameCursor]) {
                            gameRow.name[nameCursor] = s_view.names[memberIndex][nameCursor];
                            nameCursor++;
                        }

                        gameRow.name[nameCursor] = 0;
                        TileFallback(gameRow, cardX, cardDrawY, cardWidth, cardWidth, 12.0f, cardAlpha);
                    }
                }

                int shownCells = columnCount * 3;

                for (int emptyIndex = s_view.count; emptyIndex < shownCells; emptyIndex++) {
                    int gridColumn = emptyIndex % columnCount;
                    int gridRow = emptyIndex / columnCount;
                    float cardX = gridStartX + gridColumn * cardPitch;
                    float cardY = gridStartY + gridRow * (cardWidth + 33.0f);

                    if (emptyIndex == folderSel) {
                        s_hostApi->gfxPushSelectRing(cardX - 8.0f, cardY - 8.0f, cardWidth + 16.0f, cardWidth + 16.0f, 20.0f, 8.0f);
                    }

                    s_hostApi->gfxPushPanel(cardX, cardY, cardWidth, cardWidth, 12.0f,
                                   menuBarRed, menuBarGreen, menuBarBlue, 1.0f);
                }

            }
        }

        /* VGC grid. */
        {
            float gridY = 293.0f - 935.0f + shiftOffsetY;

            if (gridY > -700.0f && gridY < 1100.0f) {
                s_hostApi->fontCentered("Virtual Game Cards", 960.0f + slideOffsetX, gridY - 60.0f,
                                   44.0f, 900.0f, inkRed, inkGreen, inkBlue, 1.0f);
                int rowCount = s_hostApi->rowCount();
                int selectedIndex = s_hostApi->appSelection();
                float cardWidth = 180.0f;
                float cardGap = 20.0f;
                float cardPitch = cardWidth + cardGap;
                int columnCount = 6;
                float gridStartX = 960.0f - (columnCount * cardPitch - cardGap) * 0.5f + slideOffsetX;

                for (int cardIndex = 0; cardIndex < rowCount && cardIndex < 18; cardIndex++) {
                    int gridColumn = cardIndex % columnCount;
                    int gridRow = cardIndex / columnCount;
                    float cardX = gridStartX + gridColumn * cardPitch;
                    float cardY = gridY + gridRow * (cardWidth + 44.0f);
                    const AppRow &row = *s_hostApi->rowAt(cardIndex);
                    float cardAlpha = row.ejected ? 0.50f : 1.0f;
                    bool isSelected = (cardIndex == selectedIndex);

                    if (isSelected) {
                        s_hostApi->gfxPushSelectRing(cardX - 8.0f, cardY - 8.0f, cardWidth + 16.0f, cardWidth + 16.0f, 20.0f, 8.0f);
                    }

                    if (s_hostApi->gfxTexValid(row.iconSlot)) {
                        unsigned queuedQuads = s_hostApi->gfxPushIcon(cardX, cardY, cardWidth, cardWidth, 12.0f, 12.0f, 12.0f, 12.0f, 1.0f, 1.0f, 1.0f, 0.0f, row.iconSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, cardAlpha);

                        if (queuedQuads >= (unsigned)(kGfxIconQueueMax / 6)) {
                            TileFallback(row, cardX, cardY, cardWidth, cardWidth, 12.0f, cardAlpha);
                        }
                    } else {
                        TileFallback(row, cardX, cardY, cardWidth, cardWidth, 12.0f, cardAlpha);
                    }
                }
            }
        }
        s_hostApi->gfxPopLayer();
    }

    /// @brief Step the strip selection.
    /// A fresh press past an edge wraps around; holding into an edge
    /// clamps instead, so a continuous hold never loops.
    /// @param stepDelta Signed step.
    /// @param isFreshPress True when the direction was freshly pressed this frame.
    /// @return True when the selection changed.
    static bool StepSelection(int stepDelta, bool isFreshPress)
    {
        int rowCount = s_hostApi->rowCount();

        if (rowCount <= 0) {
            return false;
        }

        int selectedIndex = s_hostApi->appSelection();
        selectedIndex += stepDelta;

        if (selectedIndex < 0) {
            selectedIndex = isFreshPress ? rowCount - 1 : 0;
        } else if (selectedIndex >= rowCount) {
            selectedIndex = isFreshPress ? 0 : rowCount - 1;
        }

        /* Ensure we don't restart animation every time. */
        if (selectedIndex != s_hostApi->appSelection()) {
            s_hostApi->appSetSelection(selectedIndex);
            s_state.highlightProgress = 0.0f;
            s_hostApi->sfxPlay((int)Sfx::Hover);
            s_hostApi->setTopFocus(false);
            return true;
        }

        s_hostApi->setTopFocus(false);
        return false;
    }

    /// @brief Title options popup state (level 0 actions, level 1 folders).
    struct OptionsMenuState {
        bool open;
        int level;
        bool forFolder;
        bool fromFolderView;
        u64 folderId;
        u64 gameTid;
        char contextName[kAppNameMax];
        char gameVersion[32];
        char gamePublisher[kAppNameMax];
        u64 listFolders[kFolderStripMax];
        char listNames[kFolderStripMax][kAppNameMax];
        int listCount;
    };

    static OptionsMenuState s_options = { false, 0, false, false, 0, 0, { 0 }, { 0 }, { 0 }, { { 0 } }, 0 };

    /// @brief Close the title options menu without acting.
    void CloseOptions()
    {
        if (!s_options.open) {
            return;
        }

        s_options.open = false;
        s_options.level = 0;
        s_hostApi->popupClose();
    }

    /// @brief Whether the title options menu is open.
    /// @return True while the popup captures input.
    bool OptionsOpen()
    {
        return s_options.open;
    }

    /// @brief Copy a name with truncation safety.
    /// @param dst Destination buffer.
    /// @param src Source text.
    static void CopyOptionName(char *dst, const char *src)
    {
        unsigned cursor = 0;

        while (cursor + 1 < kAppNameMax && src && src[cursor]) {
            dst[cursor] = src[cursor];
            cursor++;
        }

        dst[cursor] = 0;
    }

    /// @brief Count the current level's items.
    /// @return Item count.
    static int OptionsItemCount()
    {
        if (!s_options.open) {
            return 0;
        }

        if (s_options.level == 1) {
            return 1 + s_options.listCount;
        }

        if (s_options.level == 4) {
            return kFolderColorCount;
        }

        if (s_options.level == 3) {
            return 2;
        }

        if (s_options.level == 2) {
            return 2;
        }

        if (s_options.forFolder) {
            return 4;
        }

        if (s_options.fromFolderView) {
            return 2;
        }

        return 1;
    }

    /// @brief Fill popup labels for the current options level.
    /// @param outLabels Output label pointers.
    /// @return Label count.
    static int OptionLabels(const char **outLabels)
    {
        int labelCount = 0;

        if (s_options.level == 4) {
            for (int colorIndex = 0; colorIndex < kFolderColorCount; colorIndex++) {
                outLabels[labelCount++] = kFolderColorEntries[colorIndex].name;
            }
        } else if (s_options.level == 3) {
            outLabels[labelCount++] = "Delete";
            outLabels[labelCount++] = "Cancel";
        } else if (s_options.level == 2) {
            outLabels[labelCount++] = "Create one";
            outLabels[labelCount++] = "Back";
        } else if (s_options.level == 1) {
            outLabels[labelCount++] = "New folder";

            for (int folderSlot = 0; folderSlot < s_options.listCount; folderSlot++) {
                outLabels[labelCount++] = s_options.listNames[folderSlot];
            }
        } else if (s_options.forFolder) {
            outLabels[labelCount++] = "Open";
            outLabels[labelCount++] = "Rename";
            outLabels[labelCount++] = "Change color";
            outLabels[labelCount++] = "Delete folder";
        } else if (s_options.fromFolderView) {
            outLabels[labelCount++] = "Move to folder";
            outLabels[labelCount++] = "Remove from folder";
        } else {
            outLabels[labelCount++] = "Add to folder";
        }

        return labelCount;
    }

    /// @brief Resolve a title list index from a title id.
    /// @param titleId Title id.
    /// @return Title index, or negative when absent.
    static int TitleIndexOf(u64 titleId)
    {
        if (titleId == 0) {
            return -1;
        }

        int titleCount = qext_title_count();

        if (titleCount < 0) {
            return -1;
        }

        for (int titleIndex = 0; titleIndex < titleCount; titleIndex++) {
            if (qext_title_id(titleIndex) == titleId) {
                return titleIndex;
            }
        }

        return -1;
    }

    /// @brief Fill the info lines for a game popup.
    /// @param titleId Game title id.
    static void ResolveGameInfo(u64 titleId)
    {
        s_options.gameVersion[0] = 0;
        s_options.gamePublisher[0] = 0;
        int titleIndex = TitleIndexOf(titleId);

        if (titleIndex < 0) {
            return;
        }

        if (qext_title_version(titleIndex, s_options.gameVersion, sizeof(s_options.gameVersion)) < 0) {
            s_options.gameVersion[0] = 0;
        }

        if (qext_title_publisher(titleIndex, s_options.gamePublisher, sizeof(s_options.gamePublisher)) < 0) {
            s_options.gamePublisher[0] = 0;
        }
    }

    /// @brief (Re)open the shared popup for the current options level.
    /// @param heldButtons Buttons currently held (disarmed until released).
    static void ShowOptionPopup(u64 heldButtons)
    {
        const char *labels[kPopupMaxItems];
        int labelCount = OptionLabels(labels);
        const char *titleText = s_options.contextName;

        if (s_options.level == 2) {
            titleText = "There are no folders";
        } else if (s_options.level == 3) {
            titleText = "Delete folder?";
        } else if (s_options.level == 4) {
            titleText = "Change color";
        }

        char infoMessage[kPopupTextMax];
        infoMessage[0] = 0;

        if (s_options.gameTid != 0) {
            if (s_options.gameVersion[0] != 0 && s_options.gamePublisher[0] != 0) {
                snprintf(infoMessage, sizeof(infoMessage), "Version %s - %s",
                         s_options.gameVersion, s_options.gamePublisher);
            } else if (s_options.gameVersion[0] != 0) {
                snprintf(infoMessage, sizeof(infoMessage), "Version %s", s_options.gameVersion);
            } else if (s_options.gamePublisher[0] != 0) {
                snprintf(infoMessage, sizeof(infoMessage), "%s", s_options.gamePublisher);
            }

            infoMessage[sizeof(infoMessage) - 1] = 0;
        }

        float swatchColors[kPopupMaxItems][3];
        const float *swatchList = 0;

        if (s_options.level == 4) {
            for (int colorIndex = 0; colorIndex < labelCount && colorIndex < kFolderColorCount; colorIndex++) {
                FolderPaletteColor(colorIndex, &swatchColors[colorIndex][0], &swatchColors[colorIndex][1],
                                 &swatchColors[colorIndex][2]);
            }

            swatchList = swatchColors[0];
        }

        s_hostApi->popupOpen(kPopupLayoutList, titleText, infoMessage, labels, swatchList,
                     labelCount, 0, heldButtons);

        if (s_options.level == 4) {
            s_hostApi->popupSetGridColumns(2);
        }
    }

    /// @brief Open the options menu for a strip game.
    /// @param titleId Game title id.
    /// @param titleName Game display name.
    static void OpenGameOptions(u64 titleId, const char *titleName, u64 heldButtons)
    {
        if (titleId == 0) {
            return;
        }

        s_options.open = true;
        s_options.level = 0;
        s_options.forFolder = false;
        s_options.fromFolderView = false;
        s_options.folderId = 0;
        s_options.gameTid = titleId;
        CopyOptionName(s_options.contextName, titleName);
        ResolveGameInfo(titleId);
        ShowOptionPopup(heldButtons);
        s_hostApi->sfxPlay((int)Sfx::Click);
    }

    /// @brief Open the options menu for a strip folder.
    /// @param folderId Folder id.
    /// @param folderName Folder display name.
    static void OpenFolderOptions(u64 folderId, const char *folderName, u64 heldButtons)
    {
        if (folderId == 0) {
            return;
        }

        s_options.open = true;
        s_options.level = 0;
        s_options.forFolder = true;
        s_options.fromFolderView = false;
        s_options.folderId = folderId;
        s_options.gameTid = 0;
        CopyOptionName(s_options.contextName, folderName);
        s_options.gameVersion[0] = 0;
        s_options.gamePublisher[0] = 0;
        ShowOptionPopup(heldButtons);
        s_hostApi->sfxPlay((int)Sfx::Click);
    }

    /// @brief Open the options menu for a game inside a folder view.
    /// @param titleId Member title id.
    /// @param titleName Member display name.
    static void OpenMemberOptions(u64 titleId, const char *titleName, u64 heldButtons)
    {
        if (titleId == 0) {
            return;
        }

        s_options.open = true;
        s_options.level = 0;
        s_options.forFolder = false;
        s_options.fromFolderView = true;
        s_options.folderId = 0;
        s_options.gameTid = titleId;
        CopyOptionName(s_options.contextName, titleName);
        ResolveGameInfo(titleId);
        ShowOptionPopup(heldButtons);
        s_hostApi->sfxPlay((int)Sfx::Click);
    }

    /// @brief Enter the folder-choice level for the pending game.
    static void OpenFolderChoices(u64 heldButtons)
    {
        int folderTotal = qext_folder_count();

        if (folderTotal < 0) {
            folderTotal = 0;
        }

        if (folderTotal > kFolderStripMax) {
            folderTotal = kFolderStripMax;
        }

        s_options.listCount = 0;

        for (int folderIndex = 0; folderIndex < folderTotal; folderIndex++) {
            u64 folderId = qext_folder_id(folderIndex);

            if (folderId == 0) {
                continue;
            }

            if (s_options.fromFolderView && folderId == s_hostApi->openFolder()) {
                continue;
            }

            s_options.listFolders[s_options.listCount] = folderId;
            s_options.listNames[s_options.listCount][0] = 0;

            if (qext_folder_name(folderIndex, s_options.listNames[s_options.listCount], kAppNameMax) < 0 ||
                s_options.listNames[s_options.listCount][0] == 0) {
                CopyOptionName(s_options.listNames[s_options.listCount], "Folder");
            } else {
                s_options.listNames[s_options.listCount][kAppNameMax - 1] = 0;
            }

            s_options.listCount++;
        }

        s_options.level = 1;
        ShowOptionPopup(heldButtons);
        s_hostApi->sfxPlay((int)Sfx::Click);
    }

    /// @brief Run the highlighted options action.
    static void OptionsConfirm(u64 heldButtons)
    {
        int selectedOption = s_hostApi->popupSelection();

        if (s_options.level == 4) {
            if (qext_folder_set_color(s_options.folderId, selectedOption) == 0) {
                s_hostApi->sfxPlay((int)Sfx::Click);
                CloseOptions();
                s_hostApi->refreshTitles();
            } else {
                s_hostApi->sfxPlay((int)Sfx::Back);
            }

            return;
        }

        if (s_options.level == 3) {
            if (selectedOption == 0) {
                u64 folderId = s_options.folderId;

                if (folderId != 0 && qext_folder_delete(folderId) == 0) {
                    s_hostApi->sfxPlay((int)Sfx::Click);
                    CloseOptions();
                    s_hostApi->refreshTitles();
                } else {
                    s_hostApi->sfxPlay((int)Sfx::Back);
                }
            } else {
                s_options.level = 0;
                ShowOptionPopup(heldButtons);
                s_hostApi->sfxPlay((int)Sfx::Back);
            }

            return;
        }

        if (s_options.level == 2) {
            if (selectedOption == 0) {
                u64 gameTid = s_options.gameTid;
                CloseOptions();
                CreateFolderFlow(gameTid, 0);
            } else {
                s_options.level = 0;
                ShowOptionPopup(heldButtons);
                s_hostApi->sfxPlay((int)Sfx::Back);
            }

            return;
        }

        if (s_options.level == 1) {
            if (selectedOption == 0) {
                u64 gameTid = s_options.gameTid;
                u64 removeFrom = s_options.fromFolderView ? s_hostApi->openFolder() : 0;
                CloseOptions();
                CreateFolderFlow(gameTid, removeFrom);
            } else {
                int folderSlot = selectedOption - 1;

                if (folderSlot >= 0 && folderSlot < s_options.listCount) {
                    u64 folderId = s_options.listFolders[folderSlot];
                    u64 gameTid = s_options.gameTid;

                    if (s_options.fromFolderView) {
                        u64 sourceFolder = s_hostApi->openFolder();

                        if (gameTid != 0 && folderId != 0 && sourceFolder != 0 && sourceFolder != folderId &&
                            qext_folder_remove(sourceFolder, gameTid) == 0 &&
                            qext_folder_add(folderId, gameTid) == 0) {
                            s_hostApi->sfxPlay((int)Sfx::Click);
                            CloseOptions();
                            s_hostApi->refreshTitles();

                            if (!ResolveMembers()) {
                                s_hostApi->closeFolderView();
                            } else {
                                if (s_hostApi->folderSel() >= s_view.count) {
                                    s_hostApi->setFolderSel(s_view.count - 1);
                                }

                                SyncMemberIcons();
                            }
                        } else {
                            s_hostApi->sfxPlay((int)Sfx::Back);
                        }
                    } else if (gameTid != 0 && folderId != 0 && qext_folder_add(folderId, gameTid) == 0) {
                        s_hostApi->sfxPlay((int)Sfx::Click);
                        CloseOptions();
                        s_hostApi->refreshTitles();
                        s_hostApi->selectFolderRow(folderId);
                    } else {
                        s_hostApi->sfxPlay((int)Sfx::Back);
                    }
                }
            }

            return;
        }

        if (s_options.forFolder) {
            u64 folderId = s_options.folderId;

            if (selectedOption == 0) {
                CloseOptions();
                s_hostApi->openFolderView(folderId);
            } else if (selectedOption == 1) {
                char currentName[kAppNameMax];
                CopyOptionName(currentName, s_options.contextName);
                CloseOptions();
                RenameFolderFlow(folderId, currentName);
            } else if (selectedOption == 2) {
                s_options.level = 4;
                ShowOptionPopup(heldButtons);
                s_hostApi->sfxPlay((int)Sfx::Hover);
            } else {
                s_options.level = 3;
                ShowOptionPopup(heldButtons);
                s_hostApi->sfxPlay((int)Sfx::Hover);
            }

            return;
        }

        if (s_options.fromFolderView) {
            if (selectedOption == 0) {
                OpenFolderChoices(heldButtons);
            } else {
                u64 sourceFolder = s_hostApi->openFolder();
                u64 gameTid = s_options.gameTid;

                if (sourceFolder != 0 && gameTid != 0 && qext_folder_remove(sourceFolder, gameTid) == 0) {
                    s_hostApi->sfxPlay((int)Sfx::Click);
                    CloseOptions();
                    s_hostApi->refreshTitles();

                    if (!ResolveMembers()) {
                        s_hostApi->closeFolderView();
                    } else {
                        if (s_hostApi->folderSel() >= s_view.count) {
                            s_hostApi->setFolderSel(s_view.count - 1);
                        }

                        SyncMemberIcons();
                    }
                } else {
                    s_hostApi->sfxPlay((int)Sfx::Back);
                }
            }

            return;
        }

        if (qext_folder_count() <= 0) {
            s_options.level = 2;
            ShowOptionPopup(heldButtons);
            s_hostApi->sfxPlay((int)Sfx::Hover);
        } else {
            OpenFolderChoices(heldButtons);
        }
    }

    /// @brief Handle title options popup input.
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask (reserved).
    static void OptionsInput(u64 buttonsDown, u64 buttonsHeld)
    {
        if (!s_hostApi->popupIsOpen()) {
            CloseOptions();
            return;
        }

        if (OptionsItemCount() <= 0) {
            CloseOptions();
            return;
        }

        int popupResult = s_hostApi->popupInput(buttonsDown, buttonsHeld);

        if (popupResult == kPopupResultConfirmed) {
            OptionsConfirm(buttonsHeld);
        } else if (popupResult == kPopupResultCancelled) {
            if (s_options.level == 0) {
                CloseOptions();
                s_hostApi->sfxPlay((int)Sfx::Back);
            } else {
                s_options.level = 0;
                ShowOptionPopup(buttonsHeld);
                s_hostApi->sfxPlay((int)Sfx::Back);
            }
        }
    }

    /// @brief Run the folder-name keyboard into a buffer.
    /// @param initialText Initial text (may be null or empty).
    /// @param outputBuffer Output buffer.
    /// @param outputCapacity Buffer capacity.
    /// @return Bytes written, or negative on cancel/failure.
    static int FolderNameKeyboard(const char *initialText, char *outputBuffer, unsigned outputCapacity)
    {
        SwkbdOptions keyboardOptions = { initialText, "Folder name", 1, 24, 1 };
        return qext_swkbd_show_ex(&keyboardOptions, outputBuffer, outputCapacity);
    }

    /// @brief Run the rename keyboard for a folder.
    /// @param folderId Folder id to rename.
    /// @param currentName Current name shown as initial text.
    static void RenameFolderFlow(u64 folderId, const char *currentName)
    {
        if (folderId == 0) {
            return;
        }

        char newName[128] = { 0 };

        if (FolderNameKeyboard(currentName, newName, sizeof(newName)) < 0) {
            return;
        }

        if (qext_folder_rename(folderId, newName) == 0) {
            s_hostApi->sfxPlay((int)Sfx::Click);
            s_hostApi->refreshTitles();

            if (s_hostApi->openFolder() == folderId) {
                ResolveMembers();
                SyncMemberIcons();
            }
        } else {
            s_hostApi->sfxPlay((int)Sfx::Back);
        }
    }

    /// @brief Run the create-folder keyboard for a game.
    /// @param titleId Game placed into the new folder.
    /// @param removeFromFolder Folder to remove the game from first, or 0.
    static void CreateFolderFlow(u64 titleId, u64 removeFromFolder)
    {
        if (titleId == 0) {
            return;
        }

        if (removeFromFolder != 0 && qext_folder_count() >= kFolderStripMax) {
            s_hostApi->sfxPlay((int)Sfx::Back);
            return;
        }

        char newName[128] = { 0 };

        if (FolderNameKeyboard("", newName, sizeof(newName)) < 0) {
            return;
        }

        if (removeFromFolder != 0 && qext_folder_remove(removeFromFolder, titleId) != 0) {
            s_hostApi->sfxPlay((int)Sfx::Back);
            return;
        }

        u64 folderId = qext_folder_create(newName, titleId);

        if (folderId != 0) {
            s_hostApi->sfxPlay((int)Sfx::Click);
            s_hostApi->refreshTitles();

            if (removeFromFolder != 0) {
                if (!ResolveMembers()) {
                    s_hostApi->closeFolderView();
                } else {
                    if (s_hostApi->folderSel() >= s_view.count) {
                        s_hostApi->setFolderSel(s_view.count - 1);
                    }

                    SyncMemberIcons();
                }
            } else {
                s_hostApi->selectFolderRow(folderId);
            }
        } else {
            s_hostApi->sfxPlay((int)Sfx::Back);
            s_hostApi->refreshTitles();

            if (removeFromFolder != 0 && !ResolveMembers()) {
                s_hostApi->closeFolderView();
            }
        }
    }

    /// @brief Step the folder-view selection with clamping.
    /// @param stepDelta Signed step.
    static void FolderStepSelection(int stepDelta)
    {
        if (s_view.count <= 0) {
            return;
        }

        int folderSel = s_hostApi->folderSel() + stepDelta;

        if (folderSel < 0) {
            folderSel = 0;
        }

        if (folderSel >= kFolderStripGames) {
            folderSel = kFolderStripGames - 1;
        }

        if (folderSel != s_hostApi->folderSel()) {
            s_hostApi->setFolderSel(folderSel);
            s_state.highlightProgress = 0.0f;
            s_hostApi->sfxPlay((int)Sfx::Hover);
            s_hostApi->setTopFocus(false);
        }
    }

    /// @brief Handle folders sub-screen input.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    static void HandleFoldersInput(u64 buttonsDown, u64 buttonsHeld)
    {
        if (s_options.open) {
            OptionsInput(buttonsDown, buttonsHeld);
            return;
        }

        if (!ValidateFolderView()) {
            return;
        }

        u64 filteredButtonsDown = buttonsDown;
        s_state.keyRepeat.Poll(filteredButtonsDown, buttonsHeld,
            HidNpadButton_AnyLeft | HidNpadButton_AnyRight | HidNpadButton_AnyUp | HidNpadButton_AnyDown,
            StripRepeat());

        if (filteredButtonsDown & HidNpadButton_AnyLeft) {
            FolderStepSelection(-1);
        }

        if (filteredButtonsDown & HidNpadButton_AnyRight) {
            FolderStepSelection(1);
        }

        if (filteredButtonsDown & HidNpadButton_AnyUp) {
            FolderStepSelection(-8);
        }

        if (filteredButtonsDown & HidNpadButton_AnyDown) {
            FolderStepSelection(8);
        }

        if (buttonsDown & HidNpadButton_B) {
            s_hostApi->closeFolderView();
            s_hostApi->sfxPlay((int)Sfx::Back);
            return;
        }

        if (buttonsDown & HidNpadButton_Y) {
            int folderSel = s_hostApi->folderSel();

            if (folderSel >= 0 && folderSel < s_view.count) {
                OpenMemberOptions(s_view.tids[folderSel], s_view.names[folderSel], buttonsHeld);
            }

            return;
        }

        if (buttonsDown & HidNpadButton_A) {
            int folderSel = s_hostApi->folderSel();

            if (folderSel >= 0 && folderSel < s_view.count) {
                s_hostApi->launchGameRow(s_view.tids[folderSel], s_view.ejected[folderSel], buttonsHeld);
            }
        }
    }

    /// @brief Handle VGC sub-screen input.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    static void HandleVirtualCardsInput(u64 buttonsDown, u64 buttonsHeld)
    {
        u64 filteredButtonsDown = buttonsDown;
        s_state.keyRepeat.Poll(filteredButtonsDown, buttonsHeld, HidNpadButton_AnyLeft | HidNpadButton_AnyRight, StripRepeat());

        if (filteredButtonsDown & HidNpadButton_AnyLeft) {
            StepSelection(-1, (buttonsDown & HidNpadButton_AnyLeft) != 0);
            return;
        }

        if (filteredButtonsDown & HidNpadButton_AnyRight) {
            StepSelection(1, (buttonsDown & HidNpadButton_AnyRight) != 0);
            return;
        }

        if (buttonsDown & (HidNpadButton_B | HidNpadButton_AnyUp)) {
            s_hostApi->setHomeSub(kHomeSubGames);
            s_hostApi->sfxPlay((int)Sfx::Back);
        }
    }

    /// @brief Handle games strip input.
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    static void HandleGamesInput(u64 buttonsDown, u64 buttonsHeld)
    {
        if (s_options.open) {
            OptionsInput(buttonsDown, buttonsHeld);
            return;
        }

        u64 repeatedButtonsDown = buttonsDown;
        s_state.keyRepeat.Poll(repeatedButtonsDown, buttonsHeld, HidNpadButton_AnyLeft | HidNpadButton_AnyRight, StripRepeat());

        /* No repeat reset on steps: fresh presses already reset the counter
           in Poll, and resetting here would re-arm the full hold delay for
           every step, so a hold would never actually auto-repeat. */
        if (repeatedButtonsDown & HidNpadButton_AnyLeft) {
            StepSelection(-1, (buttonsDown & HidNpadButton_AnyLeft) != 0);
        }

        if (repeatedButtonsDown & HidNpadButton_AnyRight) {
            StepSelection(1, (buttonsDown & HidNpadButton_AnyRight) != 0);
        }

        if (buttonsDown & HidNpadButton_AnyUp) {
            s_hostApi->setTopSelection(s_hostApi->appMenu());
            s_hostApi->setTopFocus(true);
            s_hostApi->sfxPlay((int)Sfx::Hover);
        }

        if (buttonsDown & HidNpadButton_AnyDown) {
            s_hostApi->setBottomSelection(0);
            s_hostApi->setBottomFocus(true);
            s_hostApi->sfxPlay((int)Sfx::Hover);
        }

        if (buttonsDown & HidNpadButton_Y) {
            int selectedIndex = s_hostApi->appSelection();

            if (selectedIndex < 0 || selectedIndex >= s_hostApi->rowCount()) {
                return;
            }

            const AppRow &selectedRow = *s_hostApi->rowAt(selectedIndex);

            if (selectedRow.isFolder) {
                OpenFolderOptions(selectedRow.folderId, selectedRow.name, buttonsHeld);
            } else if (selectedRow.isGame) {
                OpenGameOptions(selectedRow.tid, selectedRow.name, buttonsHeld);
            }
        }
    }

    /// @brief Handle home strip navigation input.
    /// @param layoutNode Gamestrip node (reserved).
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    void Input(const LayoutNode &layoutNode, u64 buttonsDown, u64 buttonsHeld)
    {
        (void)layoutNode;

        switch (CurrentPhase()) {
            case StripPhase::Hidden:
                return;

            case StripPhase::Folders:
                HandleFoldersInput(buttonsDown, buttonsHeld);
                return;

            case StripPhase::VirtualCards:
                HandleVirtualCardsInput(buttonsDown, buttonsHeld);
                return;

            case StripPhase::Games:
                HandleGamesInput(buttonsDown, buttonsHeld);
                return;
        }
    }

    /// @brief Install the host table (called once by SubmenuBoot).
    /// @param hostApi Host API implemented by menu.dnro.
    void SetHomeHost(const MenuHostApi *hostApi)
    {
        s_hostApi = hostApi;
    }

} /* namespace WStrip */

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
        return "home";
    }

    /// @brief Give the display title.
    /// @return Title owned by the dnro.
    QLAUNCH_HOOK const char *SubmenuTitle(void)
    {
        return "Home";
    }

    /// @brief Give the jingle clip name, if any.
    /// @return Clip name or NULL.
    QLAUNCH_HOOK const char *SubmenuJingle(void)
    {
        return "home_jingle";
    }

    /// @brief Give the fallback glyph, if any.
    /// @return Codepoint or 0.
    QLAUNCH_HOOK unsigned SubmenuGlyph(void)
    {
        return 0xE044;
    }

    /// @brief Give the embedded icon PNG bytes, if any.
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL.
    QLAUNCH_HOOK const unsigned char *SubmenuIconPng(unsigned *outputSize)
    {
        if (outputSize) {
            *outputSize = 0;
        }

        return nullptr;
    }

    /// @brief Give the embedded selected-state icon PNG bytes, if any.
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL to reuse the normal icon.
    QLAUNCH_HOOK const unsigned char *SubmenuSelectedIconPng(unsigned *outputSize)
    {
        if (outputSize) {
            *outputSize = 0;
        }

        return nullptr;
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

        hostApi->themeTabAccent("home", redOut, greenOut, blueOut);
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

        hostApi->themeTabColor("home", redOut, greenOut, blueOut);
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

        WStrip::SetHomeHost(hostApi);
        return 0;
    }

    /// @brief Draw the submenu onto the core's state.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuDraw(const MenuHostApi *hostApi)
    {
        LayoutNode stripNode = {};

        if (hostApi && hostApi->layoutFindNode("gamestrip", &stripNode)) {
            WStrip::Draw(stripNode);
        }

        WStrip::DrawSubs();
    }

    /// @brief Handle submenu input.
    /// @param hostApi Host API.
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    QLAUNCH_HOOK void SubmenuInput(const MenuHostApi *hostApi,
                                   unsigned long long buttonsDown,
                                   unsigned long long buttonsHeld)
    {
        LayoutNode stripNode = {};

        if (hostApi) {
            hostApi->layoutFindNode("gamestrip", &stripNode);
        }

        WStrip::Input(stripNode, buttonsDown, buttonsHeld);
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
        WStrip::CloseOptions();
    }

    /// @brief Tick the submenu once per frame.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuTick(const MenuHostApi *hostApi)
    {
        (void)hostApi;
    }

    /// @brief Whether the submenu currently covers the screen.
    /// @return Nonzero when fullscreen photo or video is showing.
    QLAUNCH_HOOK int SubmenuIsFull(void)
    {
        return 0;
    }

    /// @brief Whether the submenu wants all input routed to it.
    /// @return Nonzero when open (grid or fullscreen).
    QLAUNCH_HOOK int SubmenuInputLocked(void)
    {
        return 0;
    }

    /// @brief Drop cached textures after an external refresh.
    QLAUNCH_HOOK void SubmenuRefresh(void)
    {
        /* Previews self-refresh on folder change; nothing cached elsewhere. */
    }

    /// @brief Shut down before unload.
    QLAUNCH_HOOK void SubmenuShutdown(void)
    {
    }

} /* extern C */
