/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "qext.hpp"
#include "app.hpp"
#include "folders.hpp"
#include "la.hpp"
#include "titles.hpp"
#include "plugins/vtitles.hpp"
#include "album.hpp"
#include "sys.hpp"
#include "sysinfo.hpp"
#include <qlaunch/nro.h>
#include <shared/logging.hpp>

namespace {
    u64 g_launchedTid = 0;

    AccountUid PickUser()
    {
        AccountUid uid{};
        if (R_SUCCEEDED(accountGetLastOpenedUser(&uid)) && accountUidIsValid(&uid))
            return uid;
        AccountUid list[8] = {};
        s32 total = 0;
        if (R_SUCCEEDED(accountListAllUsers(list, 8, &total))) {
            for (s32 i = 0; i < total && i < 8; i++) {
                if (accountUidIsValid(&list[i]))
                    return list[i];
            }
        }
        AccountUid empty{};
        return empty;
    }

} // namespace

extern "C" int qext_refresh_titles(void) { return titles::Refresh(); }
extern "C" int qext_title_count(void) { return titles::Count(); }
extern "C" u64 qext_title_id(int i) { return titles::Id(i); }
extern "C" int qext_title_name(int i, char *o, unsigned c) { return titles::Name(i, o, c); }
extern "C" int qext_title_icon_size(int i) { return titles::IconSize(i); }
extern "C" int qext_title_icon(int i, void *o, unsigned c) { return titles::Icon(i, o, c); }
extern "C" int qext_title_version(int i, char *o, unsigned c) { return titles::Version(i, o, c); }
extern "C" int qext_title_publisher(int i, char *o, unsigned c) { return titles::Publisher(i, o, c); }
extern "C" int qext_title_ejected(int i) { return titles::Ejected(i); }
int qext_titles_changed(void) { return titles::TitlesChanged(); }

extern "C" int qext_album_refresh(void) { return album::Refresh(); }
extern "C" int qext_album_count(void) { return album::Count(); }
extern "C" int qext_album_thumb_size(int i) { return album::ThumbSize(i); }
extern "C" int qext_album_thumb(int i, void *o, unsigned c) { return album::Thumb(i, o, c); }
extern "C" int qext_album_image_size(int i) { return album::ImageSize(i); }
extern "C" int qext_album_image(int i, void *o, unsigned c) { return album::Image(i, o, c); }
extern "C" int qext_album_label(int i, char *o, unsigned c) { return album::Label(i, o, c); }
extern "C" int qext_album_fileid(int i, CapsAlbumFileId *o) { return album::FileId(i, o); }
extern "C" int qext_album_is_movie(int i) { return album::IsMovie(i); }
extern "C" bool qext_album_movie_open(int i)
{
    return album::MovieOpen(i);
}
extern "C" u64 qext_album_movie_size() { return album::MovieSize(); }
extern "C" int qext_album_movie_read(u64 off, void *o, unsigned c) { return album::MovieRead(off, o, c); }
extern "C" void qext_album_movie_close() { album::MovieClose(); }
extern Result heap_album_grow(void);
extern "C" int qext_album_grow(void) { return heap_album_grow(); }

extern "C" Result qext_launch_title(u64 tid)
{
    if (!tid)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);

    Result rc = 0;
    if (vtitles::IsId(tid)) {
        u64 donor = vtitles::DonorOf(tid);
        char nro[kNroPathSize];
        if (donor != 0 && vtitles::NroOf(tid, nro, sizeof(nro)) >= 0) {
            rc = nro::Launch(donor, nro, nullptr);
        } else {
            nro::Disarm();
            if (app::IsActive()) {
                rc = app::Terminate();
                if (R_FAILED(rc))
                    return rc;
            }
            rc = vtitles::Launch(tid);
        }
        if (R_SUCCEEDED(rc)) {
            titles::NoteLaunched(tid);
            g_launchedTid = tid;
        }
        return rc;
    }

    /* Any other launch scrubs leftovers so donors boot raw. */
    nro::Disarm();

    if (app::IsActive()) {
        /* Asking is for the menu */
        rc = app::Terminate();
        if (R_FAILED(rc))
            return rc;
    }

    AccountUid uid = PickUser();
    rc = app::Start(tid, false, uid);
    if (R_SUCCEEDED(rc)) {
        titles::NoteLaunched(tid);
        g_launchedTid = tid;
    }
    return rc;
}

extern "C" Result qext_resume_game(void)
{
    if (!app::IsActive())
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    appletUnlockForeground();
    return app::SetForeground();
}

extern "C" Result qext_launch_applet(int kind)
{
    if (kind < 0 || kind >= la::kHomeAppletCount)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    return la::OpenHomeApplet((la::HomeApplet)kind);
}

extern "C" int qext_swkbd_show_ex(const SwkbdOptions *options, char *out, unsigned cap)
{
    if (!out || cap < 2)
        return -1;
    out[0] = 0;

    const char *initial = options ? options->initialText : 0;
    const char *header = options ? options->headerText : 0;
    unsigned minLength = options ? options->minLength : 0;
    unsigned maxLength = options ? options->maxLength : 0;
    int blur = options ? options->blurBackground : 0;
    char result[128] = {};

    if (maxLength == 0 || maxLength > sizeof(result) - 1)
        maxLength = sizeof(result) - 1;
    if (minLength > maxLength)
        return -1;

    SwkbdConfig config{};
    Result rc = swkbdCreate(&config, 0);
    if (R_FAILED(rc)) {
        logging::LogLine("[swkbd] Can't create swkbd (rc=0x%X)", rc);
        return -1;
    }

    swkbdConfigMakePresetDefault(&config);
    if (header && header[0])
        swkbdConfigSetHeaderText(&config, header);
    if (initial && initial[0])
        swkbdConfigSetInitialText(&config, initial);
    swkbdConfigSetStringLenMax(&config, maxLength);
    swkbdConfigSetStringLenMin(&config, minLength);
    swkbdConfigSetBlurBackground(&config, blur);

    rc = swkbdShow(&config, result, sizeof(result));
    swkbdClose(&config);

    if (R_FAILED(rc)) {
        logging::LogLine("[swkbd] Can't show swkbd (rc=0x%X)", rc);
        return -1;
    }

    if (result[0] == 0)
        return -1;

    size_t n = 0;
    while (n + 1 < cap && result[n]) {
        out[n] = result[n];
        n++;
    }
    out[n] = 0;
    return (int)n;
}

extern "C" Result qext_terminate_game(void)
{
    if (!app::IsActive())
        return 0;
    return app::Terminate();
}

extern "C" int qext_folder_count(void) { return folders::Count(); }
extern "C" u64 qext_folder_id(int i) { return folders::Id(i); }
extern "C" int qext_folder_name(int i, char *o, unsigned c) { return folders::Name(i, o, c); }
extern "C" int qext_folder_game_count(int i) { return folders::GameCount(i); }
extern "C" u64 qext_folder_game(int i, int g) { return folders::Game(i, g); }
extern "C" int qext_folder_of_game(u64 tid) { return folders::IndexOfGame(tid); }
extern "C" u64 qext_folder_create(const char *n, u64 tid) { return folders::Create(n, tid); }
extern "C" int qext_folder_rename(u64 id, const char *n) { return folders::Rename(id, n); }
extern "C" int qext_folder_delete(u64 id) { return folders::Delete(id); }
extern "C" int qext_folder_add(u64 id, u64 tid) { return folders::Add(id, tid); }
extern "C" int qext_folder_remove(u64 id, u64 tid) { return folders::Remove(id, tid); }
extern "C" int qext_folder_color(int i) { return folders::Color(i); }
extern "C" int qext_folder_set_color(u64 id, int color) { return folders::SetColor(id, color); }

extern "C" Result qext_sleep(void)
{
    sys::EnterSleep();
    return 0;
}

extern "C" void qext_display_size(int *w, int *h)
{
    extern int g_fbW;
    extern int g_fbH;
    if (w)
        *w = g_fbW;
    if (h)
        *h = g_fbH;
}

extern "C" int qext_game_running(void) { return app::IsActive() ? 1 : 0; }
extern "C" int qext_game_has_foreground(void) { return app::HasForeground() ? 1 : 0; }
/* Reports the tapped identity, so a suspended forwarder matches its
   virtual tile (not its donor's real tile) and vice versa. */
extern "C" u64 qext_suspended_title(void) { return app::IsActive() ? g_launchedTid : 0; }

extern "C" int qext_account_list(AccountUid *o, int c) { return sysinfo::AccountList(o, c); }
extern "C" int qext_account_image_size(AccountUid u) { return sysinfo::AccountImageSize(u); }
extern "C" int qext_account_image(AccountUid u, void *o, unsigned c) { return sysinfo::AccountImage(u, o, c); }
extern "C" Result qext_time_now(u64 *t) { return sysinfo::TimeNow(t); }
extern "C" Result qext_time_calendar(u64 ts, TimeCalendarTime *c, TimeCalendarAdditionalInfo *i) { return sysinfo::TimeCalendar(ts, c, i); }
extern "C" int qext_battery_percent(void) { return sysinfo::BatteryPercent(); }
extern "C" int qext_battery_charging(void) { return sysinfo::BatteryCharging(); }
extern "C" int qext_wifi_signal(void) { return sysinfo::WifiSignal(); }
extern "C" int qext_airplane_mode(void) { return sysinfo::AirplaneMode(); }
extern "C" int qext_connection_type(void) { return sysinfo::ConnectionType(); }
extern "C" ColorSetId qext_colorset(void) { return sysinfo::ColorSet(); }
extern "C" int qext_account_uid_valid(AccountUid u) { return sysinfo::AccountUidValid(u); }
