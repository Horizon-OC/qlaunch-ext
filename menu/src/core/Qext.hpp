/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

/* API that is given to menus for various tasks */

#pragma once
#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

    /// @brief Refresh the cached title list from the loader.
    /// @return Title count, or < 0 on failure.
    int qext_refresh_titles(void);

    /// @brief Give the cached title count.
    /// @return Title count.
    int qext_title_count(void);

    /// @brief Give a title id by index.
    /// @param index Title index.
    /// @return Title id.
    u64 qext_title_id(int index);

    /// @brief Read a title name into a buffer.
    /// @param index Title index.
    /// @param out Output buffer.
    /// @param out_cap Buffer capacity.
    /// @return Bytes written, or < 0 on failure.
    int qext_title_name(int index, char *out, unsigned out_cap);

    /// @brief Give a title icon's byte size.
    /// @param index Title index.
    /// @return Byte size, or < 0 on failure.
    int qext_title_icon_size(int index);

    /// @brief Read a title icon into a buffer.
    /// @param index Title index.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Bytes written, or < 0 on failure.
    int qext_title_icon(int index, void *out, unsigned cap);

    /// @brief Read a title display version into a buffer.
    /// @param index Title index.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Bytes written, or < 0 when unknown.
    int qext_title_version(int index, char *out, unsigned cap);

    /// @brief Read a title publisher into a buffer.
    /// @param index Title index.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Bytes written, or < 0 when unknown.
    int qext_title_publisher(int index, char *out, unsigned cap);

    /// @brief Whether a title's game card is ejected.
    /// @param index Title index.
    /// @return > 0 when ejected.
    int qext_title_ejected(int index);

    /* Album */

    /// @brief Refresh the cached album list.
    /// @return Entry count, or < 0 on failure.
    int qext_album_refresh(void);

    /// @brief Give the cached album count.
    /// @return Entry count.
    int qext_album_count(void);

    /// @brief Give an album thumbnail's byte size.
    /// @param index Album index.
    /// @return Byte size, or < 0 on failure.
    int qext_album_thumb_size(int index);

    /// @brief Read an album thumbnail into a buffer.
    /// @param index Album index.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Bytes written, or < 0 on failure.
    int qext_album_thumb(int index, void *out, unsigned cap);

    /// @brief Give a full album image's byte size.
    /// @param index Album index.
    /// @return Byte size, or < 0 on failure.
    int qext_album_image_size(int index);

    /// @brief Read a full album image into a buffer.
    /// @param index Album index.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Bytes written, or < 0 on failure.
    int qext_album_image(int index, void *out, unsigned cap);

    /// @brief Read an album entry label into a buffer.
    /// @param index Album index.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Bytes written, or < 0 on failure.
    int qext_album_label(int index, char *out, unsigned cap);

    /// @brief Read an album entry's file id.
    /// @param index Album index.
    /// @param out Output file id.
    /// @return 0 on success, < 0 on failure.
    int qext_album_fileid(int index, CapsAlbumFileId *out);

    /// @brief Whether an album entry is a movie.
    /// @param index Album index.
    /// @return > 0 when the entry is a movie.
    int qext_album_is_movie(int index);

    /// @brief Open an album movie for streaming reads.
    /// @param index Album index.
    /// @return True on success.
    bool qext_album_movie_open(int index);

    /// @brief Give the open movie's byte size.
    /// @return Byte size, or 0 when none open.
    u64 qext_album_movie_size();

    /// @brief Read bytes from the open movie.
    /// @param offset Byte offset.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Bytes read, or < 0 on failure.
    int qext_album_movie_read(u64 offset, void *out, unsigned cap);

    /// @brief Close the open movie.
    void qext_album_movie_close();

    /// @brief Grow the album heap for large images.
    /// @return 0 on success.
    int qext_album_grow(void);

    /// @brief Launch a title by id.
    /// @param tid Title id.
    /// @return Libnx result code.
    Result qext_launch_title(u64 tid);

    /// @brief Resume the suspended game.
    /// @return Libnx result code.
    Result qext_resume_game(void);

    /// @brief Terminate the running game.
    /// @return Libnx result code.
    Result qext_terminate_game(void);

    /// @brief Put the console to sleep.
    /// @return Libnx result code.
    Result qext_sleep(void);

    /// @brief Launch a system applet by kind.
    /// @param kind Applet kind id.
    /// @return Libnx result code.
    Result qext_launch_applet(int kind);

    /* Software keyboard options for qext_swkbd_show_ex(). */
    typedef struct SwkbdOptions {
        const char *initialText; /* May be null or empty for blank. */
        const char *headerText;  /* May be null for no header. */
        unsigned minLength;      /* Minimum input length (0 = none). */
        unsigned maxLength;      /* Maximum input length (0 = internal cap). */
        int blurBackground;      /* Non-zero blurs the background behind the keyboard. */
    } SwkbdOptions;

    /// @brief Show the system keyboard; writes UTF-8 into the output buffer.
    /// @param options Keyboard options (may be null for blank defaults).
    /// @param outputBuffer Output UTF-8 buffer.
    /// @param outputCapacity Buffer capacity.
    /// @return Bytes written, or negative on cancel/failure.
    int qext_swkbd_show_ex(const SwkbdOptions *options, char *outputBuffer, unsigned outputCapacity);

    /// @brief Give the folder count.
    /// @return Folder count.
    int qext_folder_count(void);

    /// @brief Give a folder id by index.
    /// @param index Folder index.
    /// @return Folder id, or 0 when out of range.
    u64 qext_folder_id(int index);

    /// @brief Read a folder name into a buffer.
    /// @param index Folder index.
    /// @param out Output buffer.
    /// @param out_cap Buffer capacity.
    /// @return Bytes written, or negative on failure.
    int qext_folder_name(int index, char *out, unsigned out_cap);

    /// @brief Give a folder's game count.
    /// @param index Folder index.
    /// @return Game count, or negative on failure.
    int qext_folder_game_count(int index);

    /// @brief Give a member title id.
    /// @param index Folder index.
    /// @param game Member position.
    /// @return Title id, or 0 when out of range.
    u64 qext_folder_game(int index, int game);

    /// @brief Find the folder holding a game.
    /// @param tid Game title id.
    /// @return Folder index, or negative when in no folder.
    int qext_folder_of_game(u64 tid);

    /// @brief Create a folder holding one game.
    /// @param name Folder name.
    /// @param tid First game.
    /// @return Folder id, or 0 on failure.
    u64 qext_folder_create(const char *name, u64 tid);

    /// @brief Rename a folder.
    /// @param id Folder id.
    /// @param name New name.
    /// @return 0 on success, negative on failure.
    int qext_folder_rename(u64 id, const char *name);

    /// @brief Delete a folder, returning its games to the strip.
    /// @param id Folder id.
    /// @return 0 on success, negative on failure.
    int qext_folder_delete(u64 id);

    /// @brief Add a game to a folder.
    /// @param id Folder id.
    /// @param tid Game title id.
    /// @return 0 on success, negative on failure.
    int qext_folder_add(u64 id, u64 tid);

    /// @brief Remove a game from a folder.
    /// @param id Folder id.
    /// @param tid Game title id.
    /// @return 0 on success, negative on failure.
    int qext_folder_remove(u64 id, u64 tid);

    /// @brief Give a folder's palette index.
    /// @param index Folder index.
    /// @return Palette index, or negative on failure.
    int qext_folder_color(int index);

    /// @brief Set a folder's palette index.
    /// @param id Folder id.
    /// @param color Palette index.
    /// @return 0 on success, negative on failure.
    int qext_folder_set_color(u64 id, int color);

    /// @brief Give the display size in pixels.
    /// @param w Width output.
    /// @param h Height output.
    void qext_display_size(int *w, int *h);

    /// @brief Whether a game is currently running.
    /// @return Non-zero when running.
    int qext_game_running(void);

    /// @brief Whether the game holds the foreground.
    /// @return Non-zero when foregrounded.
    int qext_game_has_foreground(void);

    /// @brief Give the suspended title id.
    /// @return Title id, or 0 when none.
    u64 qext_suspended_title(void);

    /* System info (raw data; formatting stays menu-side) */

    /// @brief List user ids into out (up to cap).
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Total user count, or < 0 on failure.
    int qext_account_list(AccountUid *out, int cap);

    /// @brief Give a user's profile JPEG byte size.
    /// @param uid User id.
    /// @return Byte size, or < 0 when missing/unusable.
    int qext_account_image_size(AccountUid uid);

    /// @brief Copy a user's profile JPEG bytes.
    /// @param uid User id.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Bytes written (<0 on error).
    int qext_account_image(AccountUid uid, void *out, unsigned cap);

    /// @brief Read the user system clock.
    /// @param ts Output timestamp.
    /// @return Libnx result code.
    Result qext_time_now(u64 *ts);

    /// @brief Convert a timestamp to calendar time with the user rule.
    /// @param ts Input timestamp.
    /// @param cal Output calendar time.
    /// @param info Output additional info.
    /// @return Libnx result code.
    Result qext_time_calendar(u64 ts, TimeCalendarTime *cal,
                            TimeCalendarAdditionalInfo *info);

    /// @brief Give the battery charge percentage.
    /// @return 0-100, or < 0 when unknown.
    int qext_battery_percent(void);

    /// @brief Whether the battery is charging.
    /// @return 1 when charging, 0 when not, < 0 when unknown.
    int qext_battery_charging(void);

    /// @brief Give the WiFi signal level (radio only).
    /// @return 0 when not connected, 1-3 bars when connected,
    /// < 0 when unknown.
    int qext_wifi_signal(void);

    /* Connection type codes for qext_connection_type(). */
    enum {
        QEXT_NET_NONE = 0,     /* Not connected. */
        QEXT_NET_WIFI = 1,     /* WiFi connection is active. */
        QEXT_NET_ETHERNET = 2, /* Wired Ethernet connection is active. */
    };

    /// @brief Give the active connection type.
    /// @return QEXT_NET_* code, or < 0 when unknown.
    int qext_connection_type(void);

    /// @brief Whether airplane mode is on (wireless disabled).
    /// @return 1 when on, 0 when off, < 0 when unknown.
    int qext_airplane_mode(void);

    /// @brief Give the system color-set id, defaulting to light.
    /// @return The active color set.
    ColorSetId qext_colorset(void);

    /// @brief Test whether a user id is valid (non-empty).
    /// @param uid User id.
    /// @return Non-zero when valid.
    int qext_account_uid_valid(AccountUid uid);

#ifdef __cplusplus
}
/// @brief Whether the title list changed since last refresh.
/// @return Non-zero when changed.
int qext_titles_changed(void);
#endif

namespace logging {
    /// @brief Write a formatted line to the loader log.
    /// @param fmt printf-style format string.
    /// @param ... Format arguments.
    void LogLine(const char *fmt, ...);
}
