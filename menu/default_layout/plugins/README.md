# Menu plugins

Menu plugins are theme-pack DNROs that register buttons in the bottom bar.
Each plugin lives in its own directory here and builds to
`sdmc:/qlaunch-ext/NekoMenu/<pack>/plugins/<name>.dnro`.

A plugin exports (see `menu/src/core/MenuPluginApi.hpp`):

- `MenuPluginApiVersion` (must equal `kMenuPluginApiVersion`)
- `MenuPluginId` (lowercase, for example `vgc`)
- `MenuPluginBoot(hostApi)` (return 0 to stay loaded)
- `MenuButtonCount()`
- `MenuButtonTab(index)` (tab id like `home`, or NULL to hide)
- `MenuButtonLabel(index)` (drawn under the button)
- `MenuButtonIconPng(index, &size)` (optional; label text otherwise)
- `MenuButtonActivate(hostApi, index)` (nonzero when consumed)
- `MenuPluginShutdown()`

The layout draws one button row per active tab, right-aligned, with
the sleep button last on home. A/B and D-pad input route through the
normal bottom-bar focus; activation receives the full host API, so a
button can open popups, switch tabs, or drive its own fullscreen UI
through a companion submenu tab.

Copy a submenu `Makefile` as a starting point: same toolchain flags and
`RTLD_LOCAL` linkage, new entry points.
