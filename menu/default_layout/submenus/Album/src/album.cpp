/* NekoMenu Album submenu (C) 2026 Souldbminer */
/* Licensed under the GPLv2             */

#include "common.hpp"
#include "stb_image.h"
#include <stdlib.h>
#include <stdio.h>

namespace Album {

    namespace {

        constexpr int kAlbumColumns = 5;
        constexpr int kAlbumRows = 3;
        constexpr int kAlbumVisibleTiles = kAlbumColumns * kAlbumRows;
        constexpr int kAlbumThumbSlots = 15;

        constexpr int kAlbumThumbBytes = 0x40000;
        constexpr int kAlbumFullBytes = 0x800000;

        /// @brief Viewer phase state machine (grid vs fullscreen image).
        enum class ViewerPhase {
            Grid,
            Full,
        };

        /// @brief One image decoded into an engine texture slot.
        struct ThumbnailSlot {
            bool isUsed = false;
            int albumIndex = -1; /* entry the texture was decoded from */
            int textureSlot = -1;        /* engine texture slot, -1 when none */
            int failedIndex = -1;  /* entry that failed to decode here, -1 when none */
        };

        /// @brief Album viewer state (single file-local object).
        struct AlbumState {
            bool isOpen = false;
            bool isFresh = false; /* swallow the press that opened the viewer */
            ViewerPhase phase = ViewerPhase::Grid;
            int selectedIndex = 0;     /* selected album entry */
            int firstVisibleIndex = 0; /* top-left entry of the grid window */
            int entryCount = 0;        /* album entries from the last refresh */
            int decodeBudget = 0; /* thumbnails left to decode this frame */
            float fullShownAt = -100.0f; /* menu time when the photo hints last showed */
            InputRepeat keyRepeat = { 0 };
            ThumbnailSlot thumbnailSlots[kAlbumThumbSlots];
            ThumbnailSlot fullscreenSlot;
            uint8_t *thumbnailBuffer = 0; /* scratch for thumbnail bytes */
            uint8_t *fullscreenBuffer = 0;  /* scratch for full image bytes */
        };

        static AlbumState s_state = {};

        /// @brief Free a slot's GPU texture and detach it from its entry.
        /// @param thumbnailSlot Slot to free.
        static void FreeSlot(ThumbnailSlot *thumbnailSlot)
        {
            if (thumbnailSlot->isUsed) {
                thumbnailSlot->isUsed = false;
                thumbnailSlot->albumIndex = -1;
            }

            g_host->gfxTexFree(thumbnailSlot->textureSlot);
            thumbnailSlot->textureSlot = -1;
        }

        /// @brief Upload RGBA pixels into a slot, recycling its texture.
        /// @param thumbnailSlot Slot receiving the image.
        /// @param pixelData Pixel data.
        /// @param imageWidth Image width.
        /// @param imageHeight Image height.
        /// @return True on success.
        static bool UploadSlot(ThumbnailSlot *thumbnailSlot, const uint8_t *pixelData, int imageWidth, int imageHeight)
        {
            if (!pixelData || imageWidth <= 0 || imageHeight <= 0 || imageWidth > 1920 || imageHeight > 1080) {
                return false;
            }

            FreeSlot(thumbnailSlot);
            thumbnailSlot->textureSlot = g_host->gfxTexAlloc();

            if (thumbnailSlot->textureSlot < 0) {
                return false;
            }

            if (g_host->gfxTexUploadRgba(thumbnailSlot->textureSlot, pixelData, imageWidth, imageHeight) == 0) {
                return false;
            }

            thumbnailSlot->isUsed = true;
            return true;
        }

        /// @brief Decode JPEG bytes into fresh RGBA pixels.
        /// @param jpegData JPEG bytes.
        /// @param byteCount Byte count.
        /// @param imageWidth Width output.
        /// @param imageHeight Height output.
        /// @return Pixels to release with stbi_image_free(), or null on failure.
        static uint8_t *DecodeJpeg(const uint8_t *jpegData, int byteCount, int *imageWidth, int *imageHeight)
        {
            *imageWidth = 0;
            *imageHeight = 0;

            uint8_t *pixelData = stbi_load_from_memory(jpegData, byteCount, imageWidth, imageHeight, NULL, 4);

            if (!pixelData || *imageWidth <= 0 || *imageHeight <= 0) {
                if (pixelData) {
                    stbi_image_free(pixelData);
                }

                return NULL;
            }

            return pixelData;
        }

        /// @brief Ensure a grid thumbnail is decoded and uploaded.
        /// @param thumbnailCell Thumbnail cell position.
        /// @param albumEntryIndex Album entry index.
        /// @return True when the thumbnail is ready to draw.
        static bool EnsureThumb(int thumbnailCell, int albumEntryIndex)
        {
            ThumbnailSlot &thumbnailSlot = s_state.thumbnailSlots[thumbnailCell];

            if (thumbnailSlot.isUsed && thumbnailSlot.albumIndex == albumEntryIndex) {
                return true;
            }

            if (thumbnailSlot.failedIndex == albumEntryIndex || s_state.decodeBudget <= 0) {
                return false;
            }

            s_state.decodeBudget--;

            if (!s_state.thumbnailBuffer) {
                s_state.thumbnailBuffer = (uint8_t *)malloc(kAlbumThumbBytes);

                if (!s_state.thumbnailBuffer) {
                    return false;
                }
            }

            int byteCount = g_host->qextAlbumThumb(albumEntryIndex, s_state.thumbnailBuffer, kAlbumThumbBytes);

            if (byteCount <= 2) {
                thumbnailSlot.failedIndex = albumEntryIndex;
                LogFmt("[album] thumb %d: load got=%d", albumEntryIndex, byteCount);
                return false;
            }

            int imageWidth = 0;
            int imageHeight = 0;
            uint8_t *pixelData = DecodeJpeg(s_state.thumbnailBuffer, byteCount, &imageWidth, &imageHeight);

            if (!pixelData) {
                thumbnailSlot.failedIndex = albumEntryIndex;
                LogFmt("[album] thumb %d: jpeg decode failed (%d bytes)", albumEntryIndex, byteCount);
                return false;
            }

            thumbnailSlot.failedIndex = -1;
            bool uploadOk = UploadSlot(&thumbnailSlot, pixelData, imageWidth, imageHeight);
            stbi_image_free(pixelData);

            if (uploadOk) {
                thumbnailSlot.albumIndex = albumEntryIndex;
            }

            return uploadOk;
        }

        /// @brief Ensure the fullscreen image is decoded and uploaded.
        /// @param albumEntryIndex Album entry index.
        /// @return True when the image is ready to draw.
        static bool EnsureFull(int albumEntryIndex)
        {
            ThumbnailSlot &thumbnailSlot = s_state.fullscreenSlot;

            if (thumbnailSlot.isUsed && thumbnailSlot.albumIndex == albumEntryIndex) {
                return true;
            }

            if (thumbnailSlot.failedIndex == albumEntryIndex) {
                return false;
            }

            int neededBytes = g_host->qextAlbumImageSize(albumEntryIndex);

            if (neededBytes <= 0 || neededBytes > kAlbumFullBytes) {
                thumbnailSlot.failedIndex = albumEntryIndex;
                LogFmt("[album] full %d: size=%d", albumEntryIndex, neededBytes);
                return false;
            }

            if (!s_state.fullscreenBuffer) {
                s_state.fullscreenBuffer = (uint8_t *)malloc(kAlbumFullBytes);

                if (!s_state.fullscreenBuffer) {
                    return false;
                }
            }

            int byteCount = g_host->qextAlbumImage(albumEntryIndex, s_state.fullscreenBuffer, (unsigned)neededBytes);

            if (byteCount <= 2) {
                thumbnailSlot.failedIndex = albumEntryIndex;
                LogFmt("[album] full %d: load got=%d need=%d", albumEntryIndex, byteCount, neededBytes);
                return false;
            }

            int imageWidth = 0;
            int imageHeight = 0;
            uint8_t *pixelData = DecodeJpeg(s_state.fullscreenBuffer, byteCount, &imageWidth, &imageHeight);

            if (!pixelData) {
                thumbnailSlot.failedIndex = albumEntryIndex;
                LogFmt("[album] full %d: jpeg decode failed (%d bytes)", albumEntryIndex, byteCount);
                return false;
            }

            thumbnailSlot.failedIndex = -1;
            bool uploadOk = UploadSlot(&thumbnailSlot, pixelData, imageWidth, imageHeight);
            stbi_image_free(pixelData);

            if (uploadOk) {
                thumbnailSlot.albumIndex = albumEntryIndex;
            }

            return uploadOk;
        }

        /// @brief Free every thumbnail texture and clear its error cache.
        static void ClearThumbs()
        {
            for (int thumbnailCell = 0; thumbnailCell < kAlbumThumbSlots; thumbnailCell++) {
                FreeSlot(&s_state.thumbnailSlots[thumbnailCell]);
                s_state.thumbnailSlots[thumbnailCell].failedIndex = -1;
            }
        }

        /// @brief Clamp the selection and scroll the grid window onto it.
        static void ClampSelection()
        {
            if (s_state.entryCount <= 0) {
                s_state.selectedIndex = 0;
                s_state.firstVisibleIndex = 0;
                return;
            }

            if (s_state.selectedIndex < 0) {
                s_state.selectedIndex = 0;
            }

            if (s_state.selectedIndex >= s_state.entryCount) {
                s_state.selectedIndex = s_state.entryCount - 1;
            }

            s_state.firstVisibleIndex -= s_state.firstVisibleIndex % kAlbumColumns;

            if (s_state.firstVisibleIndex < 0) {
                s_state.firstVisibleIndex = 0;
            }

            while (s_state.selectedIndex < s_state.firstVisibleIndex) {
                s_state.firstVisibleIndex -= kAlbumColumns;
            }

            while (s_state.selectedIndex >= s_state.firstVisibleIndex + kAlbumVisibleTiles) {
                s_state.firstVisibleIndex += kAlbumColumns;
            }

            if (s_state.firstVisibleIndex < 0) {
                s_state.firstVisibleIndex = 0;
            }
        }

        /// @brief Re-query the album list, then keep the selection valid.
        static void RefreshAlbum()
        {
            s_state.entryCount = g_host->qextAlbumRefresh();
            Video::DurReset();

            if (s_state.entryCount < 0) {
                s_state.entryCount = 0;
            }

            ClampSelection();
        }

        /// @brief Enter the fullscreen phase.
        static void EnterFull()
        {
            s_state.phase = ViewerPhase::Full;
        }

        /// @brief Enter the grid phase.
        static void EnterGrid()
        {
            s_state.phase = ViewerPhase::Grid;
        }

        /// @brief Move the grid selection by a signed step, stopping at the edges.
        /// @param delta Signed step (one column, or kAlbumColumns for a row).
        static void MoveGridSelection(int delta)
        {
            int next = s_state.selectedIndex + delta;

            if (next < 0 || next >= s_state.entryCount) {
                return;
            }

            s_state.selectedIndex = next;
            ClampSelection();
            g_host->sfxPlayByName("hover");
        }

        /// @brief Step the fullscreen viewer to a neighbouring entry.
        /// @param delta Signed step.
        static void StepFullView(int delta)
        {
            int next = s_state.selectedIndex + delta;

            if (next < 0 || next >= s_state.entryCount) {
                return;
            }

            /* Movies take over playback, so fall back to the grid. */
            if (g_host->qextAlbumIsMovie(next) > 0) {
                if (!Video::Open(next)) {
                    return;
                }

                EnterGrid();
            }

            s_state.selectedIndex = next;
            ClampSelection();
            s_state.fullShownAt = g_host->appTime();
            g_host->sfxPlayByName("hover");
        }

        /// @brief Draw the duration badge over a movie thumbnail.
        /// @param albumIndex Album entry index.
        /// @param x Thumbnail X position.
        /// @param y Thumbnail Y position.
        /// @param tileW Thumbnail width.
        /// @param tileH Thumbnail height.
        static void DrawMovieBadge(int albumIndex, float x, float y, float tileW, float tileH)
        {
            if (g_host->qextAlbumIsMovie(albumIndex) <= 0) {
                return;
            }

            double duration = Video::Duration(albumIndex);

            if (duration <= 0.0) {
                return;
            }

            int minutes = (int)(duration / 60.0);
            int seconds = (int)(duration - (double)(minutes * 60));
            char label[16];
            snprintf(label, sizeof(label), "%d:%02d", minutes, seconds);

            float badgeW = g_host->fontMeasure(label, 24.0f) + 20.0f;
            float badgeX = x + tileW - badgeW - 8.0f;
            float badgeY = y + tileH - 34.0f - 8.0f;
            g_host->gfxPushPanel(badgeX, badgeY, badgeW, 34.0f, 8.0f, 0.0f, 0.0f, 0.0f, 0.6f);
            g_host->fontDraw(label, badgeX + 10.0f, badgeY + 25.0f, 24.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        }

        /// @brief Draw the grid scrollbar when more rows exist than fit.
        /// @param shiftX Horizontal layout shift.
        static void DrawScrollbar(float shiftX)
        {
            int rowCount = (s_state.entryCount + kAlbumColumns - 1) / kAlbumColumns;

            if (rowCount <= kAlbumRows) {
                return;
            }

            const float trackX = 26.0f + shiftX;
            const float trackW = 14.0f;
            const float trackTop = 240.0f;
            const float trackBottom = 830.0f;
            float trackRed = 0.0f;
            float trackGreen = 0.0f;
            float trackBlue = 0.0f;
            g_host->themeColor("menubar", &trackRed, &trackGreen, &trackBlue);
            g_host->gfxPushPanel(trackX, trackTop, trackW, trackBottom - trackTop, 7.0f, trackRed, trackGreen, trackBlue, 1.0f);

            int maxTopRow = rowCount - kAlbumRows;
            int topRow = s_state.firstVisibleIndex / kAlbumColumns;
            float thumbH = (trackBottom - trackTop) * (float)kAlbumRows / (float)rowCount;
            float thumbY = trackTop;

            if (maxTopRow > 0) {
                thumbY += (trackBottom - trackTop - thumbH) * (float)topRow / (float)maxTopRow;
            }

            g_host->gfxPushPanel(trackX + 2.0f, thumbY + 2.0f, trackW - 4.0f, thumbH - 4.0f, 5.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        }

        /// @brief Draw the thumbnail grid plus scrollbar.
        static void DrawGrid()
        {
            float dimRed = 0.0f;
            float dimGreen = 0.0f;
            float dimBlue = 0.0f;
            g_host->themeColor("dim", &dimRed, &dimGreen, &dimBlue);
            float shiftX = g_host->layoutShiftX();

            /* Opaque viewer: the home strip stays hidden underneath. */
            float bgR = 0.0f;
            float bgG = 0.0f;
            float bgB = 0.0f;
            g_host->themeTabColor("album", &bgR, &bgG, &bgB);
            g_host->gfxPushPanel(shiftX, 0.0f, 1920.0f, 1080.0f, 0.0f, bgR, bgG, bgB, 1.0f);

            if (s_state.entryCount <= 0) {
                g_host->fontCentered("No screenshots yet", 960.0f + shiftX, 500.0f, 40.0f, 1200.0f,
                                     dimRed, dimGreen, dimBlue, 1.0f);
                g_host->fontCentered("Capture with the Share button", 960.0f + shiftX, 560.0f, 32.0f,
                                     1200.0f, dimRed, dimGreen, dimBlue, 1.0f);
                return;
            }

            const float tileW = 320.0f;
            const float tileH = 180.0f;
            const float gap = 32.0f;
            const float gridW = kAlbumColumns * tileW + (kAlbumColumns - 1) * gap;
            const float gridX = (1920.0f - gridW) * 0.5f + shiftX;
            const float gridY = 240.0f;

            s_state.decodeBudget = 2;

            for (int thumbnailCell = 0; thumbnailCell < kAlbumVisibleTiles; thumbnailCell++) {
                int albumEntryIndex = s_state.firstVisibleIndex + thumbnailCell;

                if (albumEntryIndex >= s_state.entryCount) {
                    break;
                }

                int gridColumn = thumbnailCell % kAlbumColumns;
                int gridRow = thumbnailCell / kAlbumColumns;
                float tileX = gridX + gridColumn * (tileW + gap);
                float tileY = gridY + gridRow * (tileH + gap);

                if (albumEntryIndex == s_state.selectedIndex) {
                    g_host->gfxPushSelectRing(tileX - 10.0f, tileY - 10.0f, tileW + 20.0f, tileH + 20.0f, 18.0f, 8.0f);
                }

                if (EnsureThumb(thumbnailCell, albumEntryIndex)) {
                    g_host->gfxPushIcon(tileX, tileY, tileW, tileH, 8.0f, 8.0f, 8.0f, 8.0f, 1.0f, 1.0f, 1.0f, 0.0f,
                                        s_state.thumbnailSlots[thumbnailCell].textureSlot,
                                        0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                } else {
                    float emptyRed = 0.0f;
                    float emptyGreen = 0.0f;
                    float emptyBlue = 0.0f;
                    g_host->themeColor("panel", &emptyRed, &emptyGreen, &emptyBlue);
                    g_host->gfxPushPanel(tileX, tileY, tileW, tileH, 8.0f, emptyRed, emptyGreen, emptyBlue, 1.0f);
                }

                DrawMovieBadge(albumEntryIndex, tileX, tileY, tileW, tileH);
            }

            DrawScrollbar(shiftX);
        }

        /// @brief Draw one hint
        /// @param hintLabel Hint text
        /// @param alignRight True to right-align at the screen edge.
        /// @param opacity Opacity 0-1.
        static void DrawHintPill(const char *hintLabel, bool alignRight, float opacity)
        {
            float pillWidth = g_host->fontMeasure(hintLabel, 30.0f) + 48.0f;
            float pillX = alignRight ? 1860.0f - pillWidth : 60.0f;
            float pillY = 976.0f;
            g_host->gfxPushOverlayPanel(pillX, pillY, pillWidth, 64.0f, 32.0f, 0.0f, 0.0f, 0.0f, 0.6f * opacity);
            g_host->fontDraw(hintLabel, pillX + 24.0f, pillY + 44.0f, 30.0f, 1.0f, 1.0f, 1.0f, opacity);
        }

        /// @brief Draw the fullscreen photo hints
        static void DrawFullHints()
        {
            float elapsedTime = g_host->appTime() - s_state.fullShownAt;
            float hintAlpha = elapsedTime < 1.0f ? 1.0f : 1.0f - (elapsedTime - 1.0f) / 0.4f;

            if (hintAlpha <= 0.0f) {
                return;
            }

            if (hintAlpha > 1.0f) {
                hintAlpha = 1.0f;
            }

            DrawHintPill("L Previous", false, hintAlpha);
            DrawHintPill("R Next", true, hintAlpha);
        }

        /// @brief Draw the fullscreen image (or an error placeholder).
        static void DrawFull()
        {
            float inkRed = 0.0f;
            float inkGreen = 0.0f;
            float inkBlue = 0.0f;
            g_host->themeColor("ink", &inkRed, &inkGreen, &inkBlue);

            g_host->gfxPushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

            if (s_state.selectedIndex < 0 || s_state.selectedIndex >= s_state.entryCount) {
                g_host->fontCentered("No image", 960.0f, 540.0f, 40.0f, 800.0f, inkRed, inkGreen, inkBlue, 1.0f);
            } else if (EnsureFull(s_state.selectedIndex)) {
                g_host->gfxPushIcon(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,
                                    s_state.fullscreenSlot.textureSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            } else {
                g_host->fontCentered("Could not load image", 960.0f, 540.0f, 40.0f, 800.0f,
                                     1.0f, 1.0f, 1.0f, 1.0f);
            }

            DrawFullHints();
        }

    } // namespace

    /// @brief Whether the album tab is open.
    /// @return True when open.
    bool IsOpen()
    {
        return s_state.isOpen;
    }

    /// @brief Whether the album viewer covers the screen.
    /// @return True when grid-fullscreen or video is open.
    bool IsFull()
    {
        return s_state.isOpen && (s_state.phase == ViewerPhase::Full || Video::IsOpen());
    }

    /// @brief Open the album grid and refresh thumbnails.
    void Open()
    {
        int growRc = g_host->qextAlbumGrow();

        if (growRc != 0) {
            LogFmt("[album] Failed to grow heap (rc: 0x%X)", (unsigned)growRc);
        }

        s_state.entryCount = 0;

        for (int attempt = 0; attempt < 3 && s_state.entryCount <= 0; attempt++) {
            s_state.entryCount = g_host->qextAlbumRefresh();
        }

        Video::DurReset();

        LogFmt("[album] opened: %d images", s_state.entryCount);

        if (s_state.entryCount < 0) {
            s_state.entryCount = 0;
        }

        s_state.selectedIndex = 0;
        s_state.firstVisibleIndex = 0;
        s_state.phase = ViewerPhase::Grid;
        s_state.isFresh = true;

        ClearThumbs();
        FreeSlot(&s_state.fullscreenSlot);

        s_state.isOpen = true;
    }

    /// @brief Open one image fullscreen (images only, not movies).
    /// @param albumEntryIndex Album entry index.
    void OpenFull(int albumEntryIndex)
    {
        if (!s_state.isOpen || albumEntryIndex < 0 || albumEntryIndex >= s_state.entryCount) {
            return;
        }

        if (g_host->qextAlbumIsMovie(albumEntryIndex) > 0) {
            return;
        }

        s_state.selectedIndex = albumEntryIndex;
        ClampSelection();
        EnterFull();
        s_state.isFresh = false;
        s_state.fullShownAt = g_host->appTime();
        g_host->sfxPlayByName("hover");
    }

    /// @brief Drop the fullscreen image texture.
    void DropFull()
    {
        FreeSlot(&s_state.fullscreenSlot);
    }

    /// @brief Drop all thumbnail textures.
    void DropThumbs()
    {
        ClearThumbs();
    }

    /// @brief Close the album viewer and free its buffers.
    void Close()
    {
        if (Video::IsOpen()) {
            Video::Close();
        }

        Video::DropPinned();

        s_state.isOpen = false;
        s_state.phase = ViewerPhase::Grid;
        ClearThumbs();
        FreeSlot(&s_state.fullscreenSlot);

        free(s_state.thumbnailBuffer);
        s_state.thumbnailBuffer = 0;
        free(s_state.fullscreenBuffer);
        s_state.fullscreenBuffer = 0;
    }

    /// @brief Drop cached textures after an external refresh.
    void OnRefresh()
    {
        if (!s_state.isOpen) {
            return;
        }

        ClearThumbs();
        FreeSlot(&s_state.fullscreenSlot);
        EnterGrid();
    }

    /// @brief Draw the active viewer phase.
    void Draw()
    {
        if (!s_state.isOpen) {
            return;
        }

        if (Video::IsOpen()) {
            Video::Draw();
            return;
        }

        switch (s_state.phase) {
            case ViewerPhase::Full:
                DrawFull();
                break;

            case ViewerPhase::Grid:
                DrawGrid();
                break;
        }
    }

    /// @brief Handle fullscreen viewer input.
    /// @param buttonsDown Buttons pressed.
    /// @return True when the input was consumed.
    static bool HandleFullInput(u64 buttonsDown)
    {
        if (buttonsDown & HidNpadButton_B) {
            EnterGrid();
            FreeSlot(&s_state.fullscreenSlot);
            g_host->sfxPlayByName("back");
            return true;
        }

        if (buttonsDown & (HidNpadButton_AnyLeft | HidNpadButton_L | HidNpadButton_ZL)) {
            StepFullView(-1);
            return true;
        }

        if (buttonsDown & (HidNpadButton_AnyRight | HidNpadButton_R | HidNpadButton_ZR)) {
            StepFullView(1);
            return true;
        }

        if (buttonsDown & HidNpadButton_Plus) {
            RefreshAlbum();
            FreeSlot(&s_state.fullscreenSlot);
            s_state.fullShownAt = g_host->appTime();
            return true;
        }

        return true;
    }

    /// @brief Handle grid viewer input.
    /// @param buttonsDown Buttons pressed.
    /// @return True when the input was consumed.
    static bool HandleGridInput(u64 buttonsDown)
    {
        if (buttonsDown & HidNpadButton_B) {
            g_host->sfxPlayByName("back");
            g_host->switchMenuById("home");
            return true;
        }

        if (buttonsDown & HidNpadButton_A) {
            if (s_state.entryCount > 0) {
                if (g_host->qextAlbumIsMovie(s_state.selectedIndex) > 0) {
                    if (!Video::Open(s_state.selectedIndex)) {
                        g_host->sfxPlayByName("back");
                    }
                } else {
                    EnterFull();
                    g_host->sfxPlayByName("click");
                }
            }

            return true;
        }

        if (buttonsDown & HidNpadButton_Plus) {
            RefreshAlbum();
            ClearThumbs();
            return true;
        }

        int gridDelta = 0;

        if (buttonsDown & HidNpadButton_AnyLeft) {
            gridDelta = -1;
        } else if (buttonsDown & HidNpadButton_AnyRight) {
            gridDelta = 1;
        } else if (buttonsDown & HidNpadButton_AnyUp) {
            gridDelta = -kAlbumColumns;
        } else if (buttonsDown & HidNpadButton_AnyDown) {
            gridDelta = kAlbumColumns;
        }

        if (gridDelta != 0) {
            MoveGridSelection(gridDelta);
        }

        return true;
    }

    /// @brief Handle album grid/full navigation input.
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    /// @return True when the input was consumed.
    bool Input(unsigned long long buttonsDown, unsigned long long buttonsHeld)
    {
        if (!s_state.isOpen) {
            return false;
        }

        if (Video::IsOpen()) {
            return Video::Input(buttonsDown, buttonsHeld);
        }

        /* Remove any A presses if they opened the viewer. */
        if (s_state.isFresh) {
            s_state.isFresh = false;
            return true;
        }

        u64 mutableDown = (u64)buttonsDown;
        u64 mutableHeld = (u64)buttonsHeld;
        s_state.keyRepeat.Poll(mutableDown, mutableHeld,
            HidNpadButton_AnyLeft | HidNpadButton_AnyRight | HidNpadButton_AnyUp | HidNpadButton_AnyDown,
            DefaultRepeat());
        u64 provenDown = mutableDown;

        switch (s_state.phase) {
            case ViewerPhase::Full:
                return HandleFullInput(provenDown);

            case ViewerPhase::Grid:
                return HandleGridInput(provenDown);
        }

        return true;
    }

} /* namespace Album */
