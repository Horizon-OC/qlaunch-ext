# qlaunch-ext

*An fast, open source and extendable reimplementation of the Nintendo Switch home menu.*

**HEAVY WIP, DO NOT USE THIS FOR NOW!**


## Making your own menu

refer to docs/menu.md

## Theming

You have two options: Create your own menu, or theme a existing menu
Theming is implementation defined, but the default menu does support it to a degree.

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

## Troubleshooting
Ensure all DNROs in lib/ folder are present:
json.dnro
miniini.dnro
neko3d.dnro
sdl3.dnro
xxhash.dnro 

And ensure that the loader nsp is present, and your menu's dnro is present
No support will be provided for 3rd party menus unless their developers have explicitly found a backend issue

## Credits

/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

Credits, in no particular order
- XolTroll and PoloNX for uLaunch and SwitchU. Their implementation is very helpful as reference and certan parts are adapted from them <3
- etonedemid for his implementation of video playback
- toxiquid for the menu mockup
- HookedBeheamouth for their partial capsrv reimplementation (This is the only reason why videos work in the album)