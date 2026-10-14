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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../third_party/stb_image.h"

namespace WAlbum {

    namespace {

        /// @brief Viewer phase state machine (grid vs fullscreen image).
        enum ViewerPhase {
            ViewerPhase_Grid = 0,
            ViewerPhase_Full = 1,
        };

    } // namespace

static bool s_open = false;
static bool s_full = false;
static ViewerPhase s_phase = ViewerPhase_Grid;
static int s_sel = 0;
static int s_top = 0;
static int sAlbumRefreshCount = 0;
static InputRepeat s_repeat = { 0 };

constexpr int COLS = 5;
constexpr int ROWS = 3;
constexpr int VIS = COLS * ROWS;
constexpr int THUMB_SLOTS = 15;
constexpr int FULL_SLOT = 15;

struct Slot {
    bool used;
    int index;
    int tex; /* dynamic TexAlloc slot, <=2 = none */
    DkMemBlock mem;
    DkImage img;
};

static Slot s_thumb[THUMB_SLOTS];
static Slot s_fullImg;
static uint8_t *s_jpg = 0;
static uint8_t *s_big = 0;
static int s_thumbErr[THUMB_SLOTS];
static int s_fullErr = -1;
static bool s_fresh = false;
static int s_decodeBudget = 0;

    /// @brief Free a texture slot's GPU resources.
    /// @param s Slot to free.
    static void FreeSlot(Slot *s)
{
    if (s->used) {
        Gfx::WaitIdle();
        dkMemBlockDestroy(s->mem);
        s->used = false;
        s->index = -1;
        s->mem = 0;
    }
    Gfx::TexFree(s->tex);
    s->tex = -1;
}

    /// @brief Upload RGBA pixels into a texture slot.
    /// @param s Slot receiving the image.
    /// @param rgba Pixel data.
    /// @param w Width.
    /// @param h Height.
    /// @return True on success.
    static bool UploadSlot(Slot *s, const uint8_t *rgba, int w, int h)
{
    if (!rgba || w <= 0 || h <= 0 || w > 1920 || h > 1080)
        return false;
    FreeSlot(s);
    DkDevice dev = Gfx::Device();
    DkImageLayoutMaker lm;
    dkImageLayoutMakerDefaults(&lm, dev);
    lm.flags = DkImageFlags_PitchLinear;
    lm.format = DkImageFormat_RGBA8_Unorm;
    lm.dimensions[0] = (uint32_t)w;
    lm.dimensions[1] = (uint32_t)h;
    DkImageLayout lay;
    dkImageLayoutInitialize(&lay, &lm);
    uint32_t sz = dkImageLayoutGetSize(&lay);
    uint32_t al = dkImageLayoutGetAlignment(&lay);
    sz = (sz + al - 1) & ~(al - 1);
    sz = (sz + 0xFFFu) & ~0xFFFu;
    DkMemBlockMaker mm;
    dkMemBlockMakerDefaults(&mm, dev, sz);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    DkMemBlock mem = dkMemBlockCreate(&mm);
    if (!mem)
        return false;
    {
        uint32_t rowBytes = (uint32_t)w * 4u;
        uint32_t imgSize = dkImageLayoutGetSize(&lay);
        uint32_t pitch = h > 0 ? imgSize / (uint32_t)h : 0;
        uint8_t *dst = (uint8_t *)dkMemBlockGetCpuAddr(mem);
        if (pitch == rowBytes) {
            memcpy(dst, rgba, (size_t)rowBytes * (size_t)h);
        } else if (pitch > rowBytes && imgSize == pitch * (uint32_t)h) {
            for (int yy = 0; yy < h; yy++)
                memcpy(dst + (size_t)yy * pitch, rgba + (size_t)yy * rowBytes, rowBytes);
        } else {
            memcpy(dst, rgba, (size_t)rowBytes * (size_t)h);
        }
    }
    if (s->tex <= 2) {
        s->tex = Gfx::TexAlloc();
        if (s->tex < 0) {
            dkMemBlockDestroy(mem);
            return false;
        }
    }
    dkImageInitialize(&s->img, &lay, mem, 0);
    s->mem = mem;
    s->used = true;
    DkImageView view;
    dkImageViewDefaults(&view, &s->img);
    Gfx::WriteImageDesc(s->tex, &view);
    return true;
}

    /// @brief Ensure a grid thumbnail is decoded and uploaded.
    /// @param pos Thumbnail slot position.
    /// @param albumIndex Album entry index.
    /// @return True when the thumbnail is ready to draw.
    static bool EnsureThumb(int pos, int albumIndex)
{
    Slot *s = &s_thumb[pos];
    if (s->used && s->index == albumIndex) return true;
    if (s_thumbErr[pos] == albumIndex) return false;
    if (s_decodeBudget <= 0) return false;
    s_decodeBudget--;
    if (!s_jpg) {
        s_jpg = (uint8_t *)malloc(0x40000);
        if (!s_jpg)
            return false;
    }
    int got = qext_album_thumb(albumIndex, s_jpg, 0x40000);
    if (got <= 2) {
        if (s_thumbErr[pos] != albumIndex) {
            s_thumbErr[pos] = albumIndex;
            logging::LogLine("[album] thumb %d: load got=%d", albumIndex, got);
        }
        return false;
    }
    int w = 0, h = 0;
    uint8_t *px = stbi_load_from_memory(s_jpg, got, &w, &h, NULL, 4);
    if (!px || w <= 0 || h <= 0) {
        if (s_thumbErr[pos] != albumIndex) {
            s_thumbErr[pos] = albumIndex;
            logging::LogLine("[album] thumb %d: jpeg decode failed (%d bytes)", albumIndex, got);
        }
        if (px)
            stbi_image_free(px);
        return false;
    }
    s_thumbErr[pos] = -1;
    bool ok = UploadSlot(s, px, w, h);
    stbi_image_free(px);
    if (ok)
        s->index = albumIndex;
    return ok;
}

    /// @brief Ensure the fullscreen image is decoded and uploaded.
    /// @param albumIndex Album entry index.
    /// @return True when the image is ready to draw.
    static bool EnsureFull(int albumIndex)
{
    if (s_fullImg.used && s_fullImg.index == albumIndex)
        return true;
    if (s_fullErr == albumIndex) return false;
    int need = qext_album_image_size(albumIndex);
    if (need <= 0 || need > 0x800000) {
        if (s_fullErr != albumIndex) {
            s_fullErr = albumIndex;
            logging::LogLine("[album] full %d: size=%d", albumIndex, need);
        }
        return false;
    }
    if (!s_big) {
        s_big = (uint8_t *)malloc(0x800000);
        if (!s_big)
            return false;
    }
    if ((unsigned)need > 0x800000)
        return false;
    int got = qext_album_image(albumIndex, s_big, (unsigned)need);
    if (got <= 2) {
        if (s_fullErr != albumIndex) {
            s_fullErr = albumIndex;
            logging::LogLine("[album] full %d: load got=%d need=%d", albumIndex, got, need);
        }
        return false;
    }
    int w = 0, h = 0;
    uint8_t *px = stbi_load_from_memory(s_big, got, &w, &h, NULL, 4);
    if (!px || w <= 0 || h <= 0) {
        if (s_fullErr != albumIndex) {
            s_fullErr = albumIndex;
            logging::LogLine("[album] full %d: jpeg decode failed (%d bytes)", albumIndex, got);
        }
        if (px)
            stbi_image_free(px);
        return false;
    }
    s_fullErr = -1;
    bool ok = UploadSlot(&s_fullImg, px, w, h);
    stbi_image_free(px);
    if (ok)
        s_fullImg.index = albumIndex;
    return ok;
}

    /// @brief Free all thumbnail slots and clear error caches.
    static void ClearThumbs()
{
    for (int i = 0; i < THUMB_SLOTS; i++) {
        FreeSlot(&s_thumb[i]);
        s_thumbErr[i] = -1;
    }
}

    /// @brief Clamp the selection and scroll the grid window onto it.
    static void ClampSel()
{
    if (sAlbumRefreshCount <= 0) {
        s_sel = 0;
        s_top = 0;
        return;
    }
    if (s_sel < 0)
        s_sel = 0;
    if (s_sel >= sAlbumRefreshCount)
        s_sel = sAlbumRefreshCount - 1;
    s_top -= s_top % COLS;
    if (s_top < 0)
        s_top = 0;
    while (s_sel < s_top)
        s_top -= COLS;
    while (s_sel >= s_top + VIS)
        s_top += COLS;
    if (s_top < 0)
        s_top = 0;
}

    /// @brief Whether the album tab is open.
    /// @return True when open.
    bool IsOpen()
{
    return s_open;
}

    /// @brief Whether the album viewer covers the screen.
    /// @return True when grid-fullscreen or video is open.
    bool IsFull()
{
    return s_open && (s_phase == ViewerPhase_Full || WVideo::IsOpen());
}

    /// @brief Open the album grid and refresh thumbnails.
    void Open()
{
    int growRc = qext_album_grow();

    if (growRc != 0) {
        logging::LogLine("[album] Failed to grow heap (rc: 0x%X)", (unsigned)growRc);
    }

    sAlbumRefreshCount = 0;
    for (int r = 0; r < 3 && sAlbumRefreshCount <= 0; r++)
        sAlbumRefreshCount = qext_album_refresh();
        WVideo::DurReset();
    logging::LogLine("[album] opened: %d images", sAlbumRefreshCount);
    if (sAlbumRefreshCount < 0)
        sAlbumRefreshCount = 0;
    s_sel = 0;
    s_top = 0;
    s_full = false;
    s_phase = ViewerPhase_Grid;
    s_fresh = true;
    ClearThumbs();
    FreeSlot(&s_fullImg);
    WVideo::DurReset();
    s_open = true;
}

    /// @brief Open one image fullscreen (images only, not movies).
    /// @param albumIndex Album entry index.
    void OpenFull(int albumIndex)
{
    if (!s_open || albumIndex < 0 || albumIndex >= sAlbumRefreshCount) {
        return;
    }

    if (qext_album_is_movie(albumIndex) > 0) {
        return;
    }

    s_sel = albumIndex;
    ClampSel();
    s_full = true;
    s_phase = ViewerPhase_Full;
    s_fresh = false;
    Sfx::Play(Sfx::Hover);
}

    /// @brief Drop the fullscreen image texture.
    void DropFull()
{
    FreeSlot(&s_fullImg);
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

    s_open = false;
    s_full = false;
    s_phase = ViewerPhase_Grid;
    ClearThumbs();
    FreeSlot(&s_fullImg);
    free(s_jpg);
    s_jpg = 0;
    free(s_big);
    s_big = 0;
}

    /// @brief Drop cached textures after an external refresh.
    void OnRefresh()
{
    if (!s_open)
        return;
    ClearThumbs();
    FreeSlot(&s_fullImg);
    s_full = false;
    s_phase = ViewerPhase_Grid;
}

    /// @brief Draw the thumbnail grid plus scrollbar.
    static void DrawGrid()
{
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);
    float shx = Layout::ShiftX();

    /* Opaque viewer: the home strip stays hidden underneath. */
    float bgR, bgG, bgB;
    Theme::MenuColor(MENU_ALBUM, &bgR, &bgG, &bgB);
    Gfx::PushPanel(shx, 0.0f, 1920.0f, 1080.0f, 0.0f, bgR, bgG, bgB, 1.0f);

    (void)inkR;
    (void)inkG;
    (void)inkB;

    if (sAlbumRefreshCount <= 0) {
        Font::Centered("No screenshots yet", 960.0f + shx, 500.0f, 40.0f, 1200.0f,
                       dimR, dimG, dimB);
        Font::Centered("Capture with the Share button", 960.0f + shx, 560.0f, 32.0f,
                       1200.0f, dimR, dimG, dimB);
    } else {
        float tw = 320.0f, th = 180.0f, gap = 32.0f;
        float totalW = COLS * tw + (COLS - 1) * gap;
        float x0 = (1920.0f - totalW) * 0.5f + shx;
        float y0 = 240.0f;
        s_decodeBudget = 2;
        int base = s_top;
        for (int p = 0; p < VIS; p++) {
            int idx = base + p;
            if (idx >= sAlbumRefreshCount)
                break;
            int cx = p % COLS, cy = p / COLS;
            float x = x0 + cx * (tw + gap);
            float y = y0 + cy * (th + gap);
            bool sel = (idx == s_sel);
            if (sel)
                Gfx::PushSelectRing(x - 10.0f, y - 10.0f, tw + 20.0f, th + 20.0f, 18.0f, 8.0f);
            if (EnsureThumb(p, idx)) {
                Gfx::PushIcon(x, y, tw, th, 8.0f, 1.0f, 1.0f, 1.0f, 0.0f, s_thumb[p].tex);
            } else {
                Gfx::PushPanel(x, y, tw, th, 8.0f, 0.25f, 0.26f, 0.30f, 1.0f);
            }
            if (qext_album_is_movie(idx) > 0) {
                double vd = WVideo::Duration(idx);
                if (vd > 0.0) {
                    char db[16];
                    int dmm = (int)(vd / 60.0);
                    int dss = (int)(vd - (double)(dmm * 60));
                    snprintf(db, sizeof(db), "%d:%02d", dmm, dss);
                    float twn = Font::Measure(db, 24.0f);
                    float bw = twn + 20.0f;
                    float bx = x + tw - bw - 8.0f;
                    float by = y + th - 34.0f - 8.0f;
                    Gfx::PushPanel(bx, by, bw, 34.0f, 8.0f, 0.0f, 0.0f, 0.0f, 0.6f);
                    Font::Draw(db, bx + 10.0f, by + 25.0f, 24.0f, 1.0f, 1.0f, 1.0f);
                }
            }
        }
    int rows = (sAlbumRefreshCount + COLS - 1) / COLS;
    if (rows > ROWS) {
        float tx0 = 26.0f + shx, twd = 14.0f, ty0 = 240.0f, ty1 = 830.0f;
        Gfx::PushPanel(tx0, ty0, twd, ty1 - ty0, 7.0f, 0.731f, 0.731f, 0.731f, 1.0f);
        int maxTop = rows - ROWS;
        int topRow = s_top / COLS;
        float gh = (ty1 - ty0) * (float)ROWS / (float)rows;
        float gy = ty0;
        if (maxTop > 0) gy += (ty1 - ty0 - gh) * (float)topRow / (float)maxTop;
        Gfx::PushPanel(tx0 + 2.0f, gy + 2.0f, twd - 4.0f, gh - 4.0f, 5.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    }
    }
}

    /// @brief Draw the fullscreen image (or an error placeholder).
    static void DrawFull()
{
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);

    if (s_sel < 0 || s_sel >= sAlbumRefreshCount) {
        Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        Font::Centered("No image", 960.0f, 540.0f, 40.0f, 800.0f, inkR, inkG, inkB);
    } else if (EnsureFull(s_sel)) {
        Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        Gfx::PushIcon(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, s_fullImg.tex, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    } else {
        Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        Font::Centered("Could not load image", 960.0f, 540.0f, 40.0f, 800.0f,
                       1.0f, 1.0f, 1.0f);
    }
}

    /// @brief Enter the fullscreen phase.
    static void EnterFull()
    {
        s_full = true;
        s_phase = ViewerPhase_Full;
    }

    /// @brief Enter the grid phase.
    static void EnterGrid()
    {
        s_full = false;
        s_phase = ViewerPhase_Grid;
    }

    /// @brief Draw the active viewer phase.
    void Draw()
{
    if (!s_open)
        return;
    if (WVideo::IsOpen()) {
        WVideo::Draw();
        return;
    }
    if (s_phase == ViewerPhase_Full)
        DrawFull();
    else
        DrawGrid();
}

    /// @brief Handle album grid/full navigation input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    /// @return True when the input was consumed.
bool Input(u64 down, u64 held)
{
    if (!s_open)
        return false;

    if (WVideo::IsOpen()) return WVideo::Input(down, held);

    /* Remove any a presses if they opened the viewer */
    if (s_fresh) {
        s_fresh = false;
        return true;
    }

    s_repeat.Poll(down, held,
        HidNpadButton_AnyLeft | HidNpadButton_AnyRight | HidNpadButton_AnyUp | HidNpadButton_AnyDown,
        DefaultRepeat());

    if (s_phase == ViewerPhase_Full) {
        if (down & HidNpadButton_B) {
            EnterGrid();
            FreeSlot(&s_fullImg);
            Sfx::Play(Sfx::Back);
            return true;
        }
        if (down & (HidNpadButton_AnyLeft | HidNpadButton_L | HidNpadButton_ZL)) {
            if (s_sel > 0) {
                int ni = s_sel - 1;

                if (qext_album_is_movie(ni) > 0) {
                    if (WVideo::Open(ni)) {
                        s_sel = ni;
                        EnterGrid();
                        ClampSel();
                        Sfx::Play(Sfx::Hover);
                    }
                }
                else {
                    s_sel = ni;
                    ClampSel();
                    Sfx::Play(Sfx::Hover);
                }
            }
            return true;
        }
        if (down & (HidNpadButton_AnyRight | HidNpadButton_R | HidNpadButton_ZR)) {
            if (s_sel + 1 < sAlbumRefreshCount) {
                int ni = s_sel + 1;

                if (qext_album_is_movie(ni) > 0) {
                    if (WVideo::Open(ni)) {
                        s_sel = ni;
                        EnterGrid();
                        ClampSel();
                        Sfx::Play(Sfx::Hover);
                    }
                }
                else {
                    s_sel = ni;
                    ClampSel();
                    Sfx::Play(Sfx::Hover);
                }
            }
            return true;
        }
        if (down & HidNpadButton_Plus) {
            sAlbumRefreshCount = qext_album_refresh();
            WVideo::DurReset();
            if (sAlbumRefreshCount < 0)
                sAlbumRefreshCount = 0;
            ClampSel();
            FreeSlot(&s_fullImg);
            return true;
        }
        if (down & HidNpadButton_A)
            return true;
        return true;
    }

    if (down & HidNpadButton_B) {
        Sfx::Play(Sfx::Back);
        App::SwitchMenuById("home");
        return true;
    }

    if (down & HidNpadButton_A) {
        if (sAlbumRefreshCount > 0) {
            CapsAlbumFileId fid;
            int mv = qext_album_is_movie(s_sel);
            if (mv > 0) {
                if (!WVideo::Open(s_sel))
                    Sfx::Play(Sfx::Back);
            } else {
                EnterFull();
                Sfx::Play(Sfx::Click);
            }
        }
        return true;
    }

    if (down & HidNpadButton_Plus) {
        sAlbumRefreshCount = qext_album_refresh();
        WVideo::DurReset();
        if (sAlbumRefreshCount < 0)
            sAlbumRefreshCount = 0;
        ClampSel();
        ClearThumbs();
        return true;
    }

    if (down & HidNpadButton_AnyLeft) {
        if (s_sel > 0) {
            s_sel--;
            ClampSel();
            Sfx::Play(Sfx::Hover);
        }
        return true;
    }

    if (down & HidNpadButton_AnyRight) {
        if (s_sel < sAlbumRefreshCount - 1) {
            s_sel++;
            ClampSel();
            Sfx::Play(Sfx::Hover);
        }
        return true;
    }

    if (down & HidNpadButton_AnyUp) {
        if (s_sel - COLS >= 0) {
            s_sel -= COLS;
            ClampSel();
            Sfx::Play(Sfx::Hover);
        }
        return true;
    }

    if (down & HidNpadButton_AnyDown) {
        if (s_sel + COLS < sAlbumRefreshCount) {
            s_sel += COLS;
            ClampSel();
            Sfx::Play(Sfx::Hover);
        }
        return true;
    }
    
    return true;
}

} // namespace WAlbum
