/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/Layout.hpp"
#include "../core/App.hpp"
#include <stdio.h>

namespace WConnect {

/* Connect menu. Probably needs to be reworked for custom API/Friends */

void Draw()
{
    float shx = Layout::ShiftX();
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);
    Font::Draw("Friends", 120.0f + shx, 280.0f, 44.0f, inkR, inkG, inkB);

    float tw = 200.0f, gap = 50.0f;
    float x0 = 120.0f + shx;
    float y = 340.0f;
    for (int i = 0; i < 8; i++) {
        float x = x0 + (float)i * (tw + gap);
        if (x > 1900.0f)
            break;
        bool sel = (App::FriendSel() == i);
        float fr, fg, fb;
        if (sel)
            Theme::Color("accent", &fr, &fg, &fb);
        else
            Theme::Color("panel", &fr, &fg, &fb);
        Gfx::PushPanel(x, y, tw, tw, 100.0f, fr, fg, fb, 1.0f);
        char nm[16];
        snprintf(nm, sizeof(nm), "Friend %d", i + 1);
        float nw = Font::Measure(nm, 28.0f);
        Font::Draw(nm, x + (tw - nw) * 0.5f, y + tw + 40.0f, 28.0f,
                   dimR, dimG, dimB);
    }
}

void Input(u64 down, u64 held)
{
    (void)held;
    if (down & HidNpadButton_AnyLeft) {
        int s = App::FriendSel();
        if (s > 0)
            App::SetFriendSel(s - 1);
    }
    if (down & HidNpadButton_AnyRight) {
        int s = App::FriendSel();
        if (s < 7)
            App::SetFriendSel(s + 1);
    }
    if (down & HidNpadButton_AnyUp) {
        App::SetTopSel(App::Menu());
        App::SetTopFocus(true);
    }
}

} /* namespace WConnect */

namespace WEShop {

/* eShop/HBShop menu. Probably needs to be made plugin-driven. */

void Draw()
{
    float shx = Layout::ShiftX();
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);
    if (App::EShopNews()) {
        Font::Draw("News", 120.0f + shx, 280.0f, 44.0f, inkR, inkG, inkB);
        for (int i = 0; i < 4; i++) {
            float y = 350.0f + (float)i * 130.0f;
            float pr, pg, pb;
            Theme::Color("panel", &pr, &pg, &pb);
            Gfx::PushPanel(120.0f + shx, y, 1200.0f, 110.0f, 12.0f,
                           pr, pg, pb, 1.0f);
            char nm[32];
            snprintf(nm, sizeof(nm), "News article %d", i + 1);
            Font::Draw(nm, 160.0f + shx, y + 68.0f, 32.0f, dimR, dimG,
                       dimB);
        }
        Font::Draw("A: Shop", 120.0f + shx, 900.0f, 30.0f, dimR, dimG,
                   dimB);
    } else {
        Font::Draw("eShop", 120.0f + shx, 280.0f, 44.0f, inkR, inkG, inkB);
        for (int i = 0; i < 4; i++) {
            float x = 120.0f + shx + (float)i * 420.0f;
            if (x > 1800.0f)
                break;
            float pr, pg, pb;
            Theme::Color("panel", &pr, &pg, &pb);
            Gfx::PushPanel(x, 350.0f, 380.0f, 380.0f, 16.0f, pr, pg, pb,
                           1.0f);
        }
        Font::Draw("A: News", 120.0f + shx, 900.0f, 30.0f, dimR, dimG,
                   dimB);
    }
}

void Input(u64 down, u64 held)
{
    (void)held;
    if (down & HidNpadButton_A)
        App::SetEShopNews(!App::EShopNews());
    if (down & HidNpadButton_AnyUp) {
        App::SetTopSel(App::Menu());
        App::SetTopFocus(true);
    }
}

} /* namespace WEShop */