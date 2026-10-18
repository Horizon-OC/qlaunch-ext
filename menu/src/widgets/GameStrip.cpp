/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/Layout.hpp"
#include "../core/App.hpp"
#include "../core/Sfx.hpp"
#include "../core/Icons.hpp"
#include "../core/Qext.hpp"
#include "../core/Input.hpp"
#include "../core/Tabs.hpp"
#include <string.h>
#include <stdio.h>
#include <math.h>

namespace WStrip {

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
            if (App::TopFocus() || App::Menu() != Tabs::FindById("home")) {
                return StripPhase::Hidden;
            }

            if (App::HomeShiftBusy()) {
                return StripPhase::Hidden;
            }

            if (App::BottomFocus()) {
                return StripPhase::Hidden;
            }

            switch (App::HomeSub()) {
                case kHomeSubFolders:
                    return StripPhase::Folders;
                case kHomeSubVgc:
                    return StripPhase::VirtualCards;
                default:
                    return StripPhase::Games;
            }
        }

    } // namespace

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

        if (row.isGame) {
            uint32_t titleHash = (uint32_t)(row.tid ^ (row.tid >> 32));
            titleHash ^= titleHash >> 16;
            titleHash *= 0x7feb352d;
            titleHash ^= titleHash >> 15;
            titleHash *= 0x846ca68b;
            titleHash ^= titleHash >> 16;
            tileRed = 0.35f + 0.55f * ((float)((titleHash >> 0) & 0xFF) / 255.0f);
            tileGreen = 0.35f + 0.55f * ((float)((titleHash >> 8) & 0xFF) / 255.0f);
            tileBlue = 0.35f + 0.55f * ((float)((titleHash >> 16) & 0xFF) / 255.0f);
        } else if (row.applet == 0) {
            Theme::Color("tile_album", &tileRed, &tileGreen, &tileBlue);
            fallbackGlyph = GLYPH_ALBUM;
        } else {
            Theme::Color("tile_ctrl", &tileRed, &tileGreen, &tileBlue);
            fallbackGlyph = GLYPH_JOYCON;
        }

        float tileSize = tileWidth < tileHeight ? tileWidth : tileHeight;
        Gfx::PushPanel(tileX, tileY, tileWidth, tileHeight, cornerRadius, tileRed, tileGreen, tileBlue, opacity);

        if (fallbackGlyph) {
            char glyphText[4] = { 0 };
            GlyphUTF8(fallbackGlyph, glyphText);
            float glyphSize = tileSize * 0.45f;
            float glyphWidth = Font::Measure(glyphText, glyphSize);
            Font::Draw(glyphText, tileX + (tileSize - glyphWidth) * 0.5f, tileY + tileSize * 0.5f + glyphSize * 0.35f,
                       glyphSize, 1.0f, 1.0f, 1.0f);
        } else {
            const char *nameCursor = row.name;
            uint32_t codepoint = Font::Next(&nameCursor);

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
            float letterWidth = Font::Measure(initialText, letterSize);
            Font::Draw(initialText, tileX + (tileSize - letterWidth) * 0.5f,
                       tileY + tileSize * 0.5f + letterSize * 0.35f, letterSize, 1.0f, 1.0f, 1.0f);
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

        int markerSlot = Icons::HudSlot(kHudMarkerStrip);

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
        Gfx::PushIcon(markerX, markerY, markerWidth, markerHeight, 6.0f, 6.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, markerSlot, uvLeft, uvTop, uvRight, uvBottom);
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

                    if (Font::Measure(trialText, 30.0f) > 460.0f) {
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

        for (int lineIndex = 0; lineIndex < lineCount; lineIndex++) {
            Font::Centered(wrappedLines[lineIndex], centerX, topY + (float)lineIndex * 36.0f, 30.0f, 460.0f, 0.043f, 0.337f, 0.804f);
        }
    }

    /// @brief Draw the home game strip from a layout node.
    /// @param layoutNode Gamestrip node carrying tile geometry.
    void Draw(const LayoutNode &layoutNode)
    {
        float cornerRadius = 24.0f;
        float frameWidth = 6.0f;
        float tileSize = layoutNode.tile > 0.0f ? layoutNode.tile : 384.0f;
        float tileGap = layoutNode.gap > 0.0f ? layoutNode.gap : 50.0f;
        float tilePitch = tileSize + tileGap;
        float slideOffsetX = Layout::ShiftX();
        float shiftOffsetY = App::HomeShiftY();

        int rowCount = App::RowCount();
        int selectedIndex = App::Sel();

        /* Focus */
        bool isGamesActive = App::Menu() == Tabs::FindById("home") && !App::TopFocus() &&
                           !App::BottomFocus() && App::HomeSub() == kHomeSubGames;
        float outerSize = tileSize + frameWidth * 2.0f;

        /* Game label 30px accent, 461px wide. */
        if (layoutNode.label && isGamesActive && selectedIndex >= 0 && selectedIndex < rowCount) {
            float labelProgress = s_state.highlightProgress < 0.0f ? 0.0f : (s_state.highlightProgress > 1.0f ? 1.0f : s_state.highlightProgress);
            float titleCenterX = layoutNode.x + slideOffsetX + (float)selectedIndex * tilePitch - s_state.stripScroll + outerSize * 0.5f;
            DrawWrappedName(App::Row(selectedIndex).name, titleCenterX,
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
            const AppRow &row = App::Row(tileIndex);
            float tileAlpha = row.ejected ? 0.50f : 1.0f;
            float tileX = tileCenterX - outerSize * 0.5f;
            float tileY = layoutNode.y + shiftOffsetY;

            if (tileX + outerSize < -50.0f || tileX > 1970.0f) {
                continue;
            }

            if (Gfx::TexValid(row.iconSlot)) {
                unsigned queuedQuads = Gfx::PushIcon(tileX, tileY, outerSize, outerSize, cornerRadius, cornerRadius, cornerRadius, cornerRadius, 1.0f, 1.0f, 1.0f, 0.0f, row.iconSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, tileAlpha);

                if (queuedQuads >= (unsigned)(kGfxIconQueueMax / 6)) {
                    TileFallback(row, tileX, tileY, outerSize, outerSize, cornerRadius, tileAlpha);
                }
            } else {
                TileFallback(row, tileX, tileY, outerSize, outerSize, cornerRadius, tileAlpha);
            }
        }

        if (selectedTile >= 0) {
            float tileCenterX = layoutNode.x + slideOffsetX + (float)selectedTile * tilePitch - s_state.stripScroll + outerSize * 0.5f;
            const AppRow &row = App::Row(selectedTile);
            float tileAlpha = row.ejected ? 0.50f : 1.0f;
            float bottomRadius = row.ejected ? cornerRadius : 0.0f;
            float tileX = tileCenterX - outerSize * 0.5f;
            float tileY = layoutNode.y + shiftOffsetY;
            float highlightProgress = row.ejected ? 0.0f : (s_state.highlightProgress < 0.0f ? 0.0f : (s_state.highlightProgress > 1.0f ? 1.0f : s_state.highlightProgress));

            /* More gamecard stuff. */
            float growPixels = 84.0f * highlightProgress;
            float growX = tileX;
            float growY = tileY - 84.0f * highlightProgress;
            float growWidth = outerSize;
            float growHeight = outerSize + growPixels;
            Gfx::PushSelectRing(growX - 9.0f, growY - 9.0f, growWidth + 18.0f, growHeight + 18.0f, cornerRadius + 9.0f, 12.0f);

            /* Rounded top and square bottom. */
            if (Gfx::TexValid(row.iconSlot)) {
                unsigned queuedQuads = Gfx::PushIcon(growX, growY, outerSize, outerSize, cornerRadius, cornerRadius, bottomRadius, bottomRadius, 1.0f, 1.0f, 1.0f, 0.0f, row.iconSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, tileAlpha);

                if (queuedQuads >= (unsigned)(kGfxIconQueueMax / 6)) {
                    TileFallback(row, growX, growY, outerSize, outerSize, cornerRadius, tileAlpha);
                }
            } else {
                TileFallback(row, growX, growY, outerSize, outerSize, cornerRadius, tileAlpha);
            }

            if (growPixels > 0.5f) {
                Gfx::PushPanel(growX, growY + outerSize, growWidth, growPixels, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                DrawMarker(tileCenterX, growY + outerSize, growPixels, highlightProgress);
            }
        }
    }

    /// @brief Draw folders/VGC sub-screens under the strip.
    void DrawSubs()
    {
        float slideOffsetX = Layout::ShiftX();
        float shiftOffsetY = App::HomeShiftY();

        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        Theme::Color("ink", &inkRed, &inkGreen, &inkBlue);

        float dimRed = 0.0f;
        float dimGreen = 0.0f;
        float dimBlue = 0.0f;
        Theme::Color("dim", &dimRed, &dimGreen, &dimBlue);

        /* Folders screen. TODO: Fix this */
        {
            float foldersY = 293.0f + 830.0f + shiftOffsetY;

            if (foldersY > -400.0f && foldersY < 1100.0f) {
                Font::Centered("Folders", 960.0f + slideOffsetX, foldersY - 60.0f, 44.0f,
                               800.0f, inkRed, inkGreen, inkBlue);
                float folderWidth = 300.0f;
                float folderGap = 40.0f;
                float folderStartX = 960.0f - (folderWidth * 3.0f + folderGap * 2.0f) * 0.5f + slideOffsetX;
                int folderSlot = Icons::HudSlot(kHudFolder);

                for (int folderIndex = 0; folderIndex < 3; folderIndex++) {
                    char folderName[24];
                    snprintf(folderName, sizeof(folderName), "Folder %d", folderIndex + 1);
                    float folderX = folderStartX + folderIndex * (folderWidth + folderGap);

                    float panelRed = 0.0f;
                    float panelGreen = 0.0f;
                    float panelBlue = 0.0f;
                    Theme::Color("panel", &panelRed, &panelGreen, &panelBlue);
                    Gfx::PushPanel(folderX, foldersY, folderWidth, folderWidth, 24.0f, panelRed, panelGreen, panelBlue, 1.0f);

                    if (Gfx::TexValid(folderSlot)) {
                        Gfx::PushIcon(folderX + 40.0f, foldersY + 20.0f, 220.0f, 220.0f, 16.0f, 16.0f, 16.0f, 16.0f, 1.0f, 1.0f, 1.0f, 0.0f, folderSlot);
                    }

                    float nameWidth = Font::Measure(folderName, 36.0f);
                    Font::Draw(folderName, folderX + (folderWidth - nameWidth) * 0.5f, foldersY + folderWidth + 48.0f,
                               36.0f, dimRed, dimGreen, dimBlue);
                }
            }
        }

        /* VGC grid. */
        {
            float gridY = 293.0f - 935.0f + shiftOffsetY;

            if (gridY > -700.0f && gridY < 1100.0f) {
                Font::Centered("Virtual Game Cards", 960.0f + slideOffsetX, gridY - 60.0f,
                               44.0f, 900.0f, inkRed, inkGreen, inkBlue);
                int rowCount = App::RowCount();
                int selectedIndex = App::Sel();
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
                    const AppRow &row = App::Row(cardIndex);
                    float cardAlpha = row.ejected ? 0.50f : 1.0f;
                    bool isSelected = (cardIndex == selectedIndex);

                    if (isSelected) {
                        Gfx::PushSelectRing(cardX - 8.0f, cardY - 8.0f, cardWidth + 16.0f, cardWidth + 16.0f, 20.0f, 8.0f);
                    }

                    if (Gfx::TexValid(row.iconSlot)) {
                        unsigned queuedQuads = Gfx::PushIcon(cardX, cardY, cardWidth, cardWidth, 12.0f, 12.0f, 12.0f, 12.0f, 1.0f, 1.0f, 1.0f, 0.0f, row.iconSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, cardAlpha);

                        if (queuedQuads >= (unsigned)(kGfxIconQueueMax / 6)) {
                            TileFallback(row, cardX, cardY, cardWidth, cardWidth, 12.0f, cardAlpha);
                        }
                    } else {
                        TileFallback(row, cardX, cardY, cardWidth, cardWidth, 12.0f, cardAlpha);
                    }
                }
            }
        }
    }

    /// @brief Step the strip selection.
    /// A fresh press past an edge wraps around; holding into an edge
    /// clamps instead, so a continuous hold never loops.
    /// @param stepDelta Signed step.
    /// @param isFreshPress True when the direction was freshly pressed this frame.
    /// @return True when the selection changed.
    static bool StepSelection(int stepDelta, bool isFreshPress)
    {
        int rowCount = App::RowCount();

        if (rowCount <= 0) {
            return false;
        }

        int selectedIndex = App::Sel();
        selectedIndex += stepDelta;

        if (selectedIndex < 0) {
            selectedIndex = isFreshPress ? rowCount - 1 : 0;
        } else if (selectedIndex >= rowCount) {
            selectedIndex = isFreshPress ? 0 : rowCount - 1;
        }

        /* Ensure we don't restart animation every time. */
        if (selectedIndex != App::Sel()) {
            App::SetSel(selectedIndex);
            s_state.highlightProgress = 0.0f;
            Sfx::Play(Sfx::Hover);
            App::SetTopFocus(false);
            return true;
        }

        App::SetTopFocus(false);
        return false;
    }

    /// @brief Handle folders sub-screen input.
    /// @param buttonsDown Buttons pressed.
    static void HandleFoldersInput(u64 buttonsDown)
    {
        if (buttonsDown & HidNpadButton_A) {
            App::SetHomeSub(kHomeSubGames);
            Sfx::Play(Sfx::Click);
        } else if (buttonsDown & (HidNpadButton_B | HidNpadButton_AnyUp)) {
            App::SetHomeSub(kHomeSubGames);
            Sfx::Play(Sfx::Back);
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
            App::SetHomeSub(kHomeSubGames);
            Sfx::Play(Sfx::Back);
        }
    }

    /// @brief Handle games strip input.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    static void HandleGamesInput(u64 buttonsDown, u64 buttonsHeld)
    {
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
            App::SetTopSel(App::Menu());
            App::SetTopFocus(true);
            Sfx::Play(Sfx::Hover);
        }

        if (buttonsDown & HidNpadButton_AnyDown) {
            App::SetBottomSel(1);
            App::SetBottomFocus(true);
            Sfx::Play(Sfx::Hover);
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
                HandleFoldersInput(buttonsDown);
                return;

            case StripPhase::VirtualCards:
                HandleVirtualCardsInput(buttonsDown, buttonsHeld);
                return;

            case StripPhase::Games:
                HandleGamesInput(buttonsDown, buttonsHeld);
                return;
        }
    }

    /// @brief Describe the Home tab for the transplantable tab registry.
    /// @return MenuTab descriptor for Home.
    MenuTab DescribeTab()
    {
        MenuTab tabDescriptor = {};
        snprintf(tabDescriptor.id, sizeof(tabDescriptor.id), "home");
        snprintf(tabDescriptor.title, sizeof(tabDescriptor.title), "Home");
        tabDescriptor.glyph = 0xE044;
        snprintf(tabDescriptor.jingle, sizeof(tabDescriptor.jingle), "home_jingle");
        tabDescriptor.draw = nullptr;
        tabDescriptor.input = nullptr;
        tabDescriptor.onEnter = nullptr;
        tabDescriptor.onExit = nullptr;
        return tabDescriptor;
    }

} /* namespace WStrip */
