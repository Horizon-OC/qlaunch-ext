/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once

enum { MENU_HOME = 0, MENU_CONNECT, MENU_ESHOP, MENU_ALBUM, MENU_SETTINGS, MENU_COUNT };

namespace Theme {
    /// @brief Load a legacy single-theme JSON blob.
    /// @param json JSON text.
    /// @param len Text length.
    /// @return True on success.
    bool LoadJson(const char *json, unsigned len);

    /// @brief Load a builtin theme by name.
    /// @param name "light" or "dark".
    /// @return True on success.
    bool LoadBuiltin(const char *name); /* "light" / "dark" */

    /// @brief Load a theme override from an SD file.
    /// @param path Filesystem path (64KB max).
    /// @return True on success.
    bool LoadFile(const char *path);    /* SD override, 64KB max */

    /// @brief Load the SD theme index, else the system color set.
    /// @return True on success.
    bool InitAuto();                    /* SD theme index, else system set */

    /// @brief Look up a theme color.
    /// @param key Color key (for example "ink").
    /// @param r Red output.
    /// @param g Green output.
    /// @param b Blue output.
    void Color(const char *key, float *r, float *g, float *b);

    /// @brief Look up a theme metric.
    /// @param key Metric key (for example "corner").
    /// @param dflt Default when missing.
    /// @return Metric value or default.
    float Metric(const char *key, float dflt);

    /// @brief Give the active theme name.
    /// @return Theme name.
    const char *Name();

    /// @brief Number of themes in the table.
    /// @return Theme count.
    int ThemeCount();

    /// @brief Look up a theme name by index.
    /// @param i Theme index.
    /// @return Theme name, or "" when out of range.
    const char *ThemeName(int i);

    /// @brief Activate a theme by index.
    /// @param i Theme index.
    /// @return True on success.
    bool SetTheme(int i);

    /// @brief Give the active theme index.
    /// @return Theme index.
    int GetTheme();

    /// @brief Whether the active theme is dark.
    /// @return True for dark themes.
    bool IsDark();

    /// @brief Give a tab's background color.
    /// @param menu Tab index.
    /// @param r Red output.
    /// @param g Green output.
    /// @param b Blue output.
    void MenuColor(int menu, float *r, float *g, float *b);

    /// @brief Give a tab's accent color.
    /// @param menu Tab index.
    /// @param r Red output.
    /// @param g Green output.
    /// @param b Blue output.
    void MenuAccent(int menu, float *r, float *g, float *b);

    /// @brief Give a theme's preview swatch color.
    /// @param t Theme index.
    /// @param r Red output.
    /// @param g Green output.
    /// @param b Blue output.
    void ThemePreview(int t, float *r, float *g, float *b);
} /* namespace Theme */
