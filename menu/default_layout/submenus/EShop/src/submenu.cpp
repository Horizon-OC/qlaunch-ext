/* NekoMenu EShop submenu (C) 2026 Souldbminer */
/* Licensed under the GPLv2              */

#include <switch.h>
#include "MenuHost.hpp"
#include "SubmenuApi.hpp"
#include <cstdio>
#include <stdint.h>

extern const uint8_t top_sel_eshop_png[];
extern const uint32_t top_sel_eshop_png_size;
extern const uint8_t top_des_eshop_png[];
extern const uint32_t top_des_eshop_png_size;

namespace {

    constexpr int kEshopCardCount = 4;

    enum class EshopView {
        ShopGrid,
        NewsList,
    };

    struct EshopState {
        bool showNews;
    };

    static EshopState s_state = { false };

    /// @brief Draw the shop grid.
    /// @param hostApi Host API.
    /// @param slideOffsetX Horizontal slide offset.
    void DrawShopGrid(const MenuHostApi *hostApi, float slideOffsetX)
    {
        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        hostApi->themeColor("ink", &inkRed, &inkGreen, &inkBlue);

        float dimRed = 0.0f;
        float dimGreen = 0.0f;
        float dimBlue = 0.0f;
        hostApi->themeColor("dim", &dimRed, &dimGreen, &dimBlue);

        hostApi->fontDraw("eShop", 120.0f + slideOffsetX, 280.0f, 44.0f, inkRed, inkGreen, inkBlue, 1.0f);

        for (int cardIndex = 0; cardIndex < kEshopCardCount; cardIndex++) {
            float cardX = 120.0f + slideOffsetX + (float)cardIndex * 420.0f;

            if (cardX > 1800.0f) {
                break;
            }

            float panelRed = 0.0f;
            float panelGreen = 0.0f;
            float panelBlue = 0.0f;
            hostApi->themeColor("panel", &panelRed, &panelGreen, &panelBlue);
            hostApi->gfxPushPanel(cardX, 350.0f, 380.0f, 380.0f, 16.0f, panelRed, panelGreen, panelBlue, 1.0f);
        }

        hostApi->fontDraw("A: News", 120.0f + slideOffsetX, 900.0f, 30.0f, dimRed, dimGreen, dimBlue, 1.0f);
    }

    /// @brief Draw the news list.
    /// @param hostApi Host API.
    /// @param slideOffsetX Horizontal slide offset.
    void DrawNewsList(const MenuHostApi *hostApi, float slideOffsetX)
    {
        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        hostApi->themeColor("ink", &inkRed, &inkGreen, &inkBlue);

        float dimRed = 0.0f;
        float dimGreen = 0.0f;
        float dimBlue = 0.0f;
        hostApi->themeColor("dim", &dimRed, &dimGreen, &dimBlue);

        hostApi->fontDraw("News", 120.0f + slideOffsetX, 280.0f, 44.0f, inkRed, inkGreen, inkBlue, 1.0f);

        for (int articleIndex = 0; articleIndex < kEshopCardCount; articleIndex++) {
            float articleY = 350.0f + (float)articleIndex * 130.0f;

            float panelRed = 0.0f;
            float panelGreen = 0.0f;
            float panelBlue = 0.0f;
            hostApi->themeColor("panel", &panelRed, &panelGreen, &panelBlue);
            hostApi->gfxPushPanel(120.0f + slideOffsetX, articleY, 1200.0f, 110.0f, 12.0f,
                                  panelRed, panelGreen, panelBlue, 1.0f);

            char articleTitle[32];
            snprintf(articleTitle, sizeof(articleTitle), "News article %d", articleIndex + 1);
            hostApi->fontDraw(articleTitle, 160.0f + slideOffsetX, articleY + 68.0f, 32.0f,
                              dimRed, dimGreen, dimBlue, 1.0f);
        }

        hostApi->fontDraw("A: Shop", 120.0f + slideOffsetX, 900.0f, 30.0f, dimRed, dimGreen, dimBlue, 1.0f);
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
        return "eshop";
    }

    /// @brief Give the display title.
    /// @return Title owned by the dnro.
    QLAUNCH_HOOK const char *SubmenuTitle(void)
    {
        return "Shop";
    }

    /// @brief Give the jingle clip name, if any.
    /// @return Clip name or NULL.
    QLAUNCH_HOOK const char *SubmenuJingle(void)
    {
        return "eshop_jingle";
    }

    /// @brief Give the fallback glyph, if any.
    /// @return Codepoint or 0.
    QLAUNCH_HOOK unsigned SubmenuGlyph(void)
    {
        return 0xE133;
    }

    /// @brief Give the embedded icon PNG bytes, if any.
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL.
    QLAUNCH_HOOK const unsigned char *SubmenuIconPng(unsigned *outputSize)
    {
        if (outputSize) {
            *outputSize = top_sel_eshop_png_size;
        }

        return top_sel_eshop_png;
    }

    /// @brief Give the embedded selected-state icon PNG bytes, if any.
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL to reuse the normal icon.
    QLAUNCH_HOOK const unsigned char *SubmenuSelectedIconPng(unsigned *outputSize)
    {
        if (outputSize) {
            *outputSize = top_des_eshop_png_size;
        }

        return top_des_eshop_png;
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

        hostApi->themeTabAccent("eshop", redOut, greenOut, blueOut);
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

        hostApi->themeTabColor("eshop", redOut, greenOut, blueOut);
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

        s_state.showNews = false;
        return 0;
    }

    /// @brief Draw the submenu onto the core's state.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuDraw(const MenuHostApi *hostApi)
    {
        float slideOffsetX = hostApi->layoutShiftX();

        switch (s_state.showNews ? EshopView::NewsList : EshopView::ShopGrid) {
            case EshopView::NewsList:
                DrawNewsList(hostApi, slideOffsetX);
                break;

            case EshopView::ShopGrid:
                DrawShopGrid(hostApi, slideOffsetX);
                break;
        }
    }

    /// @brief Handle submenu input.
    /// @param hostApi Host API.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    QLAUNCH_HOOK void SubmenuInput(const MenuHostApi *hostApi,
                                   unsigned long long buttonsDown,
                                   unsigned long long buttonsHeld)
    {
        (void)buttonsHeld;

        if (buttonsDown & (unsigned long long)HidNpadButton_A) {
            s_state.showNews = !s_state.showNews;
            hostApi->sfxPlayByName("click");
        }

        if (buttonsDown & (unsigned long long)HidNpadButton_AnyUp) {
            hostApi->setTopSelection(hostApi->appMenu());
            hostApi->setTopFocus(1);
            hostApi->sfxPlayByName("hover");
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
