/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once

constexpr int kMenuHome = 0;
constexpr int kMenuConnect = 1;
constexpr int kMenuEshop = 2;
constexpr int kMenuAlbum = 3;
constexpr int kMenuSettings = 4;
constexpr int kMenuCount = 5;

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
    /// @param menu Theme slot 0-4.
    /// @param r Red output.
    /// @param g Green output.
    /// @param b Blue output.
    void MenuColor(int menu, float *r, float *g, float *b);

    /// @brief Shut down theme state and free retained file bytes.
    void Shutdown();

    /// @brief Give the current theme file bytes.
    /// @param outputData Output file pointer, valid until the next theme change.
    /// @param outputSize Output byte size.
    /// @return True when a theme file is retained.
    bool ThemeFile(const unsigned char **outputData, unsigned *outputSize);

    /// @brief Resolve a tab id to a theme background slot.
    /// @param tabId Tab id (for example "album").
    /// @return 0-4 for known tabs, or -1 when unknown.
    int MenuSlotForId(const char *tabId);

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
