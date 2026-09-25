/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Theme.hpp"
#include "../core/App.hpp"

namespace WBackground {

void Draw(const LayoutNode &nd)
{
    (void)nd;
    int m = App::MenuT() < 1.0f ? App::MenuFrom() : App::Menu();
    float r, g, b;
    Theme::MenuColor(m, &r, &g, &b);
    Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, r, g, b, 1.0f);
}

} /* namespace WBackground */