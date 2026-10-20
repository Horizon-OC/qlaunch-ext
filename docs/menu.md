# Making a menu

Refer to the existing menu to see how it is built and linked.
You have a good amount of capability as a menu, and can control the entire lifecycle

You have two options for creating menu/custom interfaces as well:
1. Use the existing NekoMenu as a host (easier)
2. Write a completely custom menu implementation (harder)

For most cases I reccomend using NekoMenu as it provides a rendering and sound API, alongside management and is very customizable

But if you want to make your own menu you can refer to NekoMenu code or look at the menu interface

## Making your own menu

Rendering:
You have many options for rendering:
neko3d
SDL3 with neko3d backend (2D via SDL, 3D via neko3d)
Software (libnx framebuffer)

Input:
You can use SDL3 or libnx for input

Sound:
Again, SDL3 or libnx

Programming language:
C++, but bindings for Lua and JavaScript are planned in future

## Making your own extention to NekoMenu
TODO