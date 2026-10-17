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
#include "../core/Qext.hpp"
#include "../core/Input.hpp"
#include "../core/Tabs.hpp"
#include <stdlib.h>
#include <stdio.h>

#include "../third_party/stb_image.h"

namespace WAlbum {

    namespace {

        constexpr int kColumns = 5;
        constexpr int kRows = 3;
        constexpr int kVisible = kColumns * kRows;
        constexpr int kThumbSlots = 15;

        constexpr int kThumbBytes = 0x40000;
        constexpr int kFullBytes = 0x800000;

        /// @brief Viewer phase state machine (grid vs fullscreen image).
        enum class ViewerPhase {
            Grid,
            Full,
        };

        /// @brief One image decoded into an engine texture slot.
        struct Slot {
            bool used = false;
            int albumIndex = -1; /* entry the texture was decoded from */
            int tex = -1;        /* engine texture slot, -1 when none */
            int failIndex = -1;  /* entry that failed to decode here, -1 when none */
        };

        /// @brief Album viewer state (single file-local object).
        struct State {
            bool open = false;
            bool fresh = false; /* swallow the press that opened the viewer */
            ViewerPhase phase = ViewerPhase::Grid;
            int selected = 0;     /* selected album entry */
            int firstVisible = 0; /* top-left entry of the grid window */
            int count = 0;        /* album entries from the last refresh */
            int decodeBudget = 0; /* thumbnails left to decode this frame */
            InputRepeat repeat = { 0 };
            Slot thumbs[kThumbSlots];
            Slot full;
            uint8_t *thumbBuffer = 0; /* scratch for thumbnail bytes */
            uint8_t *fullBuffer = 0;  /* scratch for full image bytes */
        };

        static State s_state = {};

        /// @brief Free a slot's GPU texture and detach it from its entry.
        /// @param slot Slot to free.
        static void FreeSlot(Slot *slot)
        {
            if (slot->used) {
                slot->used = false;
                slot->albumIndex = -1;
            }

            Gfx::TexFree(slot->tex);
            slot->tex = -1;
        }

        /// @brief Upload RGBA pixels into a slot, recycling its texture.
        /// @param slot Slot receiving the image.
        /// @param rgba Pixel data.
        /// @param width Image width.
        /// @param height Image height.
        /// @return True on success.
        static bool UploadSlot(Slot *slot, const uint8_t *rgba, int width, int height)
        {
            if (!rgba || width <= 0 || height <= 0 || width > 1920 || height > 1080) {
                return false;
            }

            FreeSlot(slot);
            slot->tex = Gfx::TexAlloc();

            if (slot->tex < 0) {
                return false;
            }

            if (!Gfx::TexUpload(slot->tex, rgba, width, height, Gfx::TexFormat::RGBA)) {
                return false;
            }

            slot->used = true;
            return true;
        }

        /// @brief Decode JPEG bytes into fresh RGBA pixels.
        /// @param jpeg JPEG bytes.
        /// @param bytes Byte count.
        /// @param width Width output.
        /// @param height Height output.
        /// @return Pixels to release with stbi_image_free(), or null on failure.
        static uint8_t *DecodeJpeg(const uint8_t *jpeg, int bytes, int *width, int *height)
        {
            *width = 0;
            *height = 0;

            uint8_t *pixels = stbi_load_from_memory(jpeg, bytes, width, height, NULL, 4);

            if (!pixels || *width <= 0 || *height <= 0) {
                if (pixels) {
                    stbi_image_free(pixels);
                }

                return NULL;
            }

            return pixels;
        }

        /// @brief Ensure a grid thumbnail is decoded and uploaded.
        /// @param cell Thumbnail cell position.
        /// @param albumIndex Album entry index.
        /// @return True when the thumbnail is ready to draw.
        static bool EnsureThumb(int cell, int albumIndex)
        {
            Slot &slot = s_state.thumbs[cell];

            if (slot.used && slot.albumIndex == albumIndex) {
                return true;
            }

            if (slot.failIndex == albumIndex || s_state.decodeBudget <= 0) {
                return false;
            }

            s_state.decodeBudget--;

            if (!s_state.thumbBuffer) {
                s_state.thumbBuffer = (uint8_t *)malloc(kThumbBytes);

                if (!s_state.thumbBuffer) {
                    return false;
                }
            }

            int bytes = qext_album_thumb(albumIndex, s_state.thumbBuffer, kThumbBytes);

            if (bytes <= 2) {
                slot.failIndex = albumIndex;
                logging::LogLine("[album] thumb %d: load got=%d", albumIndex, bytes);
                return false;
            }

            int width = 0;
            int height = 0;
            uint8_t *pixels = DecodeJpeg(s_state.thumbBuffer, bytes, &width, &height);

            if (!pixels) {
                slot.failIndex = albumIndex;
                logging::LogLine("[album] thumb %d: jpeg decode failed (%d bytes)", albumIndex, bytes);
                return false;
            }

            slot.failIndex = -1;
            bool ok = UploadSlot(&slot, pixels, width, height);
            stbi_image_free(pixels);

            if (ok) {
                slot.albumIndex = albumIndex;
            }

            return ok;
        }

        /// @brief Ensure the fullscreen image is decoded and uploaded.
        /// @param albumIndex Album entry index.
        /// @return True when the image is ready to draw.
        static bool EnsureFull(int albumIndex)
        {
            Slot &slot = s_state.full;

            if (slot.used && slot.albumIndex == albumIndex) {
                return true;
            }

            if (slot.failIndex == albumIndex) {
                return false;
            }

            int needed = qext_album_image_size(albumIndex);

            if (needed <= 0 || needed > kFullBytes) {
                slot.failIndex = albumIndex;
                logging::LogLine("[album] full %d: size=%d", albumIndex, needed);
                return false;
            }

            if (!s_state.fullBuffer) {
                s_state.fullBuffer = (uint8_t *)malloc(kFullBytes);

                if (!s_state.fullBuffer) {
                    return false;
                }
            }

            int bytes = qext_album_image(albumIndex, s_state.fullBuffer, (unsigned)needed);

            if (bytes <= 2) {
                slot.failIndex = albumIndex;
                logging::LogLine("[album] full %d: load got=%d need=%d", albumIndex, bytes, needed);
                return false;
            }

            int width = 0;
            int height = 0;
            uint8_t *pixels = DecodeJpeg(s_state.fullBuffer, bytes, &width, &height);

            if (!pixels) {
                slot.failIndex = albumIndex;
                logging::LogLine("[album] full %d: jpeg decode failed (%d bytes)", albumIndex, bytes);
                return false;
            }

            slot.failIndex = -1;
            bool ok = UploadSlot(&slot, pixels, width, height);
            stbi_image_free(pixels);

            if (ok) {
                slot.albumIndex = albumIndex;
            }

            return ok;
        }

        /// @brief Free every thumbnail texture and clear its error cache.
        static void ClearThumbs()
        {
            for (int cell = 0; cell < kThumbSlots; cell++) {
                FreeSlot(&s_state.thumbs[cell]);
                s_state.thumbs[cell].failIndex = -1;
            }
        }

        /// @brief Clamp the selection and scroll the grid window onto it.
        static void ClampSelection()
        {
            if (s_state.count <= 0) {
                s_state.selected = 0;
                s_state.firstVisible = 0;
                return;
            }

            if (s_state.selected < 0) {
                s_state.selected = 0;
            }

            if (s_state.selected >= s_state.count) {
                s_state.selected = s_state.count - 1;
            }

            s_state.firstVisible -= s_state.firstVisible % kColumns;

            if (s_state.firstVisible < 0) {
                s_state.firstVisible = 0;
            }

            while (s_state.selected < s_state.firstVisible) {
                s_state.firstVisible -= kColumns;
            }

            while (s_state.selected >= s_state.firstVisible + kVisible) {
                s_state.firstVisible += kColumns;
            }

            if (s_state.firstVisible < 0) {
                s_state.firstVisible = 0;
            }
        }

        /// @brief Re-query the album list, then keep the selection valid.
        static void RefreshAlbum()
        {
            s_state.count = qext_album_refresh();
            WVideo::DurReset();

            if (s_state.count < 0) {
                s_state.count = 0;
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
        /// @param delta Signed step (one column, or kColumns for a row).
        static void MoveGridSelection(int delta)
        {
            int next = s_state.selected + delta;

            if (next < 0 || next >= s_state.count) {
                return;
            }

            s_state.selected = next;
            ClampSelection();
            Sfx::Play(Sfx::Hover);
        }

        /// @brief Step the fullscreen viewer to a neighbouring entry.
        /// @param delta Signed step.
        static void StepFullView(int delta)
        {
            int next = s_state.selected + delta;

            if (next < 0 || next >= s_state.count) {
                return;
            }

            /* Movies take over playback, so fall back to the grid. */
            if (qext_album_is_movie(next) > 0) {
                if (!WVideo::Open(next)) {
                    return;
                }

                EnterGrid();
            }

            s_state.selected = next;
            ClampSelection();
            Sfx::Play(Sfx::Hover);
        }

        /// @brief Draw the duration badge over a movie thumbnail.
        /// @param albumIndex Album entry index.
        /// @param x Thumbnail X position.
        /// @param y Thumbnail Y position.
        /// @param tileW Thumbnail width.
        /// @param tileH Thumbnail height.
        static void DrawMovieBadge(int albumIndex, float x, float y, float tileW, float tileH)
        {
            if (qext_album_is_movie(albumIndex) <= 0) {
                return;
            }

            double duration = WVideo::Duration(albumIndex);

            if (duration <= 0.0) {
                return;
            }

            int minutes = (int)(duration / 60.0);
            int seconds = (int)(duration - (double)(minutes * 60));
            char label[16];
            snprintf(label, sizeof(label), "%d:%02d", minutes, seconds);

            float badgeW = Font::Measure(label, 24.0f) + 20.0f;
            float badgeX = x + tileW - badgeW - 8.0f;
            float badgeY = y + tileH - 34.0f - 8.0f;
            Gfx::PushPanel(badgeX, badgeY, badgeW, 34.0f, 8.0f, 0.0f, 0.0f, 0.0f, 0.6f);
            Font::Draw(label, badgeX + 10.0f, badgeY + 25.0f, 24.0f, 1.0f, 1.0f, 1.0f);
        }

        /// @brief Draw the grid scrollbar when more rows exist than fit.
        /// @param shiftX Horizontal layout shift.
        static void DrawScrollbar(float shiftX)
        {
            int rowCount = (s_state.count + kColumns - 1) / kColumns;

            if (rowCount <= kRows) {
                return;
            }

            const float trackX = 26.0f + shiftX;
            const float trackW = 14.0f;
            const float trackTop = 240.0f;
            const float trackBottom = 830.0f;
            Gfx::PushPanel(trackX, trackTop, trackW, trackBottom - trackTop, 7.0f, 0.731f, 0.731f, 0.731f, 1.0f);

            int maxTopRow = rowCount - kRows;
            int topRow = s_state.firstVisible / kColumns;
            float thumbH = (trackBottom - trackTop) * (float)kRows / (float)rowCount;
            float thumbY = trackTop;

            if (maxTopRow > 0) {
                thumbY += (trackBottom - trackTop - thumbH) * (float)topRow / (float)maxTopRow;
            }

            Gfx::PushPanel(trackX + 2.0f, thumbY + 2.0f, trackW - 4.0f, thumbH - 4.0f, 5.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        }

        /// @brief Draw the thumbnail grid plus scrollbar.
        static void DrawGrid()
        {
            float dimR, dimG, dimB;
            Theme::Color("dim", &dimR, &dimG, &dimB);
            float shiftX = Layout::ShiftX();

            /* Opaque viewer: the home strip stays hidden underneath. */
            float bgR, bgG, bgB;
            Theme::MenuColor(MENU_ALBUM, &bgR, &bgG, &bgB);
            Gfx::PushPanel(shiftX, 0.0f, 1920.0f, 1080.0f, 0.0f, bgR, bgG, bgB, 1.0f);

            if (s_state.count <= 0) {
                Font::Centered("No screenshots yet", 960.0f + shiftX, 500.0f, 40.0f, 1200.0f,
                               dimR, dimG, dimB);
                Font::Centered("Capture with the Share button", 960.0f + shiftX, 560.0f, 32.0f,
                               1200.0f, dimR, dimG, dimB);
                return;
            }

            const float tileW = 320.0f;
            const float tileH = 180.0f;
            const float gap = 32.0f;
            const float gridW = kColumns * tileW + (kColumns - 1) * gap;
            const float gridX = (1920.0f - gridW) * 0.5f + shiftX;
            const float gridY = 240.0f;

            s_state.decodeBudget = 2;

            for (int cell = 0; cell < kVisible; cell++) {
                int albumIndex = s_state.firstVisible + cell;

                if (albumIndex >= s_state.count) {
                    break;
                }

                int col = cell % kColumns;
                int row = cell / kColumns;
                float x = gridX + col * (tileW + gap);
                float y = gridY + row * (tileH + gap);

                if (albumIndex == s_state.selected) {
                    Gfx::PushSelectRing(x - 10.0f, y - 10.0f, tileW + 20.0f, tileH + 20.0f, 18.0f, 8.0f);
                }

                if (EnsureThumb(cell, albumIndex)) {
                    Gfx::PushIcon(x, y, tileW, tileH, 8.0f, 8.0f, 8.0f, 8.0f, 1.0f, 1.0f, 1.0f, 0.0f,
                                  s_state.thumbs[cell].tex);
                } else {
                    Gfx::PushPanel(x, y, tileW, tileH, 8.0f, 0.25f, 0.26f, 0.30f, 1.0f);
                }

                DrawMovieBadge(albumIndex, x, y, tileW, tileH);
            }

            DrawScrollbar(shiftX);
        }

        /// @brief Draw the fullscreen image (or an error placeholder).
        static void DrawFull()
        {
            float inkR, inkG, inkB;
            Theme::Color("ink", &inkR, &inkG, &inkB);

            Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

            if (s_state.selected < 0 || s_state.selected >= s_state.count) {
                Font::Centered("No image", 960.0f, 540.0f, 40.0f, 800.0f, inkR, inkG, inkB);
            } else if (EnsureFull(s_state.selected)) {
                Gfx::PushIcon(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,
                              s_state.full.tex, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
            } else {
                Font::Centered("Could not load image", 960.0f, 540.0f, 40.0f, 800.0f,
                               1.0f, 1.0f, 1.0f);
            }
        }

    } // namespace

    /// @brief Whether the album tab is open.
    /// @return True when open.
    bool IsOpen()
    {
        return s_state.open;
    }

    /// @brief Whether the album viewer covers the screen.
    /// @return True when grid-fullscreen or video is open.
    bool IsFull()
    {
        return s_state.open && (s_state.phase == ViewerPhase::Full || WVideo::IsOpen());
    }

    /// @brief Open the album grid and refresh thumbnails.
    void Open()
    {
        int growRc = qext_album_grow();

        if (growRc != 0) {
            logging::LogLine("[album] Failed to grow heap (rc: 0x%X)", (unsigned)growRc);
        }

        s_state.count = 0;

        for (int attempt = 0; attempt < 3 && s_state.count <= 0; attempt++) {
            s_state.count = qext_album_refresh();
        }

        WVideo::DurReset();

        logging::LogLine("[album] opened: %d images", s_state.count);

        if (s_state.count < 0) {
            s_state.count = 0;
        }

        s_state.selected = 0;
        s_state.firstVisible = 0;
        s_state.phase = ViewerPhase::Grid;
        s_state.fresh = true;

        ClearThumbs();
        FreeSlot(&s_state.full);

        s_state.open = true;
    }

    /// @brief Open one image fullscreen (images only, not movies).
    /// @param albumIndex Album entry index.
    void OpenFull(int albumIndex)
    {
        if (!s_state.open || albumIndex < 0 || albumIndex >= s_state.count) {
            return;
        }

        if (qext_album_is_movie(albumIndex) > 0) {
            return;
        }

        s_state.selected = albumIndex;
        ClampSelection();
        EnterFull();
        s_state.fresh = false;
        Sfx::Play(Sfx::Hover);
    }

    /// @brief Drop the fullscreen image texture.
    void DropFull()
    {
        FreeSlot(&s_state.full);
    }

    /// @brief Drop all thumbnail textures.
    void DropThumbs()
    {
        ClearThumbs();
    }

    /// @brief Close the album viewer and free its buffers.
    void Close()
    {
        if (WVideo::IsOpen()) {
            WVideo::Close();
        }

        WVideo::DropPinned();

        s_state.open = false;
        s_state.phase = ViewerPhase::Grid;
        ClearThumbs();
        FreeSlot(&s_state.full);

        free(s_state.thumbBuffer);
        s_state.thumbBuffer = 0;
        free(s_state.fullBuffer);
        s_state.fullBuffer = 0;
    }

    /// @brief Drop cached textures after an external refresh.
    void OnRefresh()
    {
        if (!s_state.open) {
            return;
        }

        ClearThumbs();
        FreeSlot(&s_state.full);
        EnterGrid();
    }

    /// @brief Draw the active viewer phase.
    void Draw()
    {
        if (!s_state.open) {
            return;
        }

        if (WVideo::IsOpen()) {
            WVideo::Draw();
            return;
        }

        if (s_state.phase == ViewerPhase::Full) {
            DrawFull();
        } else {
            DrawGrid();
        }
    }

    /// @brief Handle album grid/full navigation input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    /// @return True when the input was consumed.
    bool Input(u64 down, u64 held)
    {
        if (!s_state.open) {
            return false;
        }

        if (WVideo::IsOpen()) {
            return WVideo::Input(down, held);
        }

        /* Remove any A presses if they opened the viewer. */
        if (s_state.fresh) {
            s_state.fresh = false;
            return true;
        }

        s_state.repeat.Poll(down, held,
            HidNpadButton_AnyLeft | HidNpadButton_AnyRight | HidNpadButton_AnyUp | HidNpadButton_AnyDown,
            DefaultRepeat());

        if (s_state.phase == ViewerPhase::Full) {
            if (down & HidNpadButton_B) {
                EnterGrid();
                FreeSlot(&s_state.full);
                Sfx::Play(Sfx::Back);
                return true;
            }

            if (down & (HidNpadButton_AnyLeft | HidNpadButton_L | HidNpadButton_ZL)) {
                StepFullView(-1);
                return true;
            }

            if (down & (HidNpadButton_AnyRight | HidNpadButton_R | HidNpadButton_ZR)) {
                StepFullView(1);
                return true;
            }

            if (down & HidNpadButton_Plus) {
                RefreshAlbum();
                FreeSlot(&s_state.full);
                return true;
            }

            return true;
        }

        if (down & HidNpadButton_B) {
            Sfx::Play(Sfx::Back);
            App::SwitchMenuById("home");
            return true;
        }

        if (down & HidNpadButton_A) {
            if (s_state.count > 0) {
                if (qext_album_is_movie(s_state.selected) > 0) {
                    if (!WVideo::Open(s_state.selected)) {
                        Sfx::Play(Sfx::Back);
                    }
                } else {
                    EnterFull();
                    Sfx::Play(Sfx::Click);
                }
            }

            return true;
        }

        if (down & HidNpadButton_Plus) {
            RefreshAlbum();
            ClearThumbs();
            return true;
        }

        int gridDelta = 0;

        if (down & HidNpadButton_AnyLeft) {
            gridDelta = -1;
        } else if (down & HidNpadButton_AnyRight) {
            gridDelta = 1;
        } else if (down & HidNpadButton_AnyUp) {
            gridDelta = -kColumns;
        } else if (down & HidNpadButton_AnyDown) {
            gridDelta = kColumns;
        }

        if (gridDelta != 0) {
            MoveGridSelection(gridDelta);
        }

        return true;
    }

} // namespace WAlbum
