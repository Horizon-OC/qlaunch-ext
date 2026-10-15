# qlaunch-ext

*An fast, open source and extendable reimplementation of the Nintendo Switch home menu.*

**HEAVY WIP, DO NOT USE THIS FOR NOW!**


## Making your own menu

You can use the existing menu as a base here

To boot a different menu, drop its dnro under sdmc:/qlaunch-ext/menus/ and set it to boot like so
sdmc:/qlaunch-ext/config/menu.ini:
[menu]
path=(the path of the menu)

By default sdmc:/qlaunch-ext/menus/menu.dnro is booted.

## Theming

TODO

## Plugins

- Inbuilt:
    - MTPPlugin: MTP server that runs all the time
    - ForwarderPlugin: Put your homebrew on the home menu with *zero* ban risk

- TODO:
    - Ultrahand: ultrahand as a plugin, so it can use applet ram
    - FTPPlugin: FTP server
    - JellyfinPlugin: Replace album with Jellyfin client
    - PluginPlugin: Plugin manager plugin
    - AppstorePlugin: HB apppstore integration
    - SocialPlugin: Social feature from a selfhosted server
    - EmulationPlugin: Depends on FakeForwarderPlugin, emulator integration
    - FileManagerPlugin: Self explanitory

## Homebrew support
This is only officially supported on FW >16.0.0
All homebrew should be supported.

## Credits

/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

Credits, in no particular order
- XolTroll and PoloNX for uLaunch and SwitchU. Their implementation is very helpful as reference and certan parts are adapted from them <3
- toxiquid for the menu mockup
- HookedBeheamouth for their partial capsrv reimplementation (This is the only reason why videos work in the album)