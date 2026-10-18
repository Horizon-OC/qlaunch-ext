/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

int qext_refresh_titles(void);
int qext_title_count(void);
u64 qext_title_id(int index);
int qext_title_name(int index, char *out, unsigned out_cap);
int qext_title_icon_size(int index);
int qext_title_icon(int index, void *out, unsigned cap);
int qext_title_version(int index, char *out, unsigned cap);
int qext_title_publisher(int index, char *out, unsigned cap);

int qext_album_refresh(void);
int qext_album_count(void);
int qext_album_thumb_size(int index);
int qext_album_thumb(int index, void *out, unsigned cap);
int qext_album_image_size(int index);
int qext_album_image(int index, void *out, unsigned cap);
int qext_album_label(int index, char *out, unsigned cap);
int qext_album_fileid(int index, CapsAlbumFileId *out);

int qext_folder_count(void);
u64 qext_folder_id(int index);
int qext_folder_name(int index, char *out, unsigned cap);
int qext_folder_game_count(int index);
u64 qext_folder_game(int index, int game);
int qext_folder_of_game(u64 tid);
u64 qext_folder_create(const char *name, u64 tid);
int qext_folder_rename(u64 id, const char *name);
int qext_folder_delete(u64 id);
int qext_folder_add(u64 id, u64 tid);
int qext_folder_remove(u64 id, u64 tid);
int qext_folder_color(int index);
int qext_folder_set_color(u64 id, int color);

Result qext_launch_title(u64 tid);
Result qext_resume_game(void);
Result qext_terminate_game(void);
Result qext_sleep(void);
/* Open a home-menu applet: 0 album, 1 controllers, 2 mii editor. */
Result qext_launch_applet(int kind);

typedef struct SwkbdOptions {
    const char *initialText; /* May be null or empty for blank. */
    const char *headerText;  /* May be null for no header. */
    unsigned minLength;      /* Minimum input length (0 = none). */
    unsigned maxLength;      /* Maximum input length (0 = internal cap). */
    int blurBackground;      /* Non-zero blurs the background behind the keyboard. */
} SwkbdOptions;

int qext_swkbd_show_ex(const SwkbdOptions *options, char *out, unsigned cap);
void qext_display_size(int *w, int *h);
int qext_game_running(void);
int qext_game_has_foreground(void);
u64 qext_suspended_title(void);

#ifdef __cplusplus
}
#endif