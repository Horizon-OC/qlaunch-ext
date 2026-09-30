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
int qext_title_ejected(int index);

/* Album */
int qext_album_refresh(void);
int qext_album_count(void);
int qext_album_thumb_size(int index);
int qext_album_thumb(int index, void *out, unsigned cap);
int qext_album_image_size(int index);
int qext_album_image(int index, void *out, unsigned cap);
int qext_album_label(int index, char *out, unsigned cap);
int qext_album_fileid(int index, CapsAlbumFileId *out);
int qext_album_is_movie(int index);
bool qext_album_movie_open(int index);
u64 qext_album_movie_size();
int qext_album_movie_read(u64 offset, void *out, unsigned cap);
void qext_album_movie_close();
int qext_album_grow(void);

Result qext_launch_title(u64 tid);
Result qext_resume_game(void);
Result qext_terminate_game(void);
Result qext_sleep(void);
Result qext_launch_applet(int kind);
void qext_display_size(int *w, int *h);
int qext_game_running(void);
int qext_game_has_foreground(void);
u64 qext_suspended_title(void);

Result timeGetCurrentTime(TimeType type, u64 *timestamp);
Result timeToCalendarTimeWithMyRule(u64 timestamp, TimeCalendarTime *caltime,
                                    TimeCalendarAdditionalInfo *info);
Result psmGetBatteryChargePercentage(u32 *out);
Result setsysGetColorSetId(ColorSetId *out);

#ifdef __cplusplus
}
int qext_titles_changed(void);
#endif

namespace logging {
void LogLine(const char *fmt, ...);
}
