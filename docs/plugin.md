# Making a plugin

Plugins are very capable parts of qlaunch-ext.

There are several types of plugins each of which have different functionality
- LoaderPlugin
- MenuPlugin

First, LoaderPlugins.
LoaderPlugins hook the loader (the part of qlaunch-ext that loads the menu). They can call various loader functions and have full access to libnx and other libs.

To create one you can use the plugin tempalate (example/)
It has a makefile and main.cpp set up for creatinon of a plugin

Then, for MenuPlugins.
MenuPlugins are defined by the menu implementation. For NekoMenu (the inbuilt menu), support is provided