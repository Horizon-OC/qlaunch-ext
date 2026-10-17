/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "sysinfo.hpp"
#include <shared/logging.hpp>

namespace sysinfo {

namespace {

constexpr unsigned kImageCap = 0x40000;

} // namespace

int AccountList(AccountUid *out, int cap)
{
    if (!out || cap <= 0)
        return -1;
    s32 total = 0;
    if (R_FAILED(accountListAllUsers(out, cap, &total)))
        return -1;
    return (int)total;
}

int AccountImageSize(AccountUid uid)
{
    AccountProfile prof{};
    if (R_FAILED(accountGetProfile(&prof, uid)))
        return -1;
    u32 size = 0;
    Result rc = accountProfileGetImageSize(&prof, &size);
    accountProfileClose(&prof);
    if (R_FAILED(rc) || size <= 2 || size > kImageCap)
        return -1;
    return (int)size;
}

int AccountImage(AccountUid uid, void *out, unsigned cap)
{
    int want = AccountImageSize(uid);
    if (want < 0 || !out || (unsigned)want > cap)
        return -1;
    AccountProfile prof{};
    if (R_FAILED(accountGetProfile(&prof, uid)))
        return -1;
    u8 *jpg = (u8 *)out;
    u32 done = 0;
    Result rc = accountProfileLoadImage(&prof, jpg, (u32)want, &done);
    accountProfileClose(&prof);
    if (R_FAILED(rc) || done <= 2 || done > (u32)want)
        return -1;
    return (int)done;
}

Result TimeNow(u64 *ts)
{
    if (!ts)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    return timeGetCurrentTime(TimeType_UserSystemClock, ts);
}

Result TimeCalendar(u64 ts, TimeCalendarTime *cal, TimeCalendarAdditionalInfo *info)
{
    if (!cal || !info)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    return timeToCalendarTimeWithMyRule(ts, cal, info);
}

int BatteryPercent()
{
    u32 pct = 0;
    if (R_FAILED(psmGetBatteryChargePercentage(&pct)) || pct > 100)
        return -1;
    return (int)pct;
}

int BatteryCharging()
{
    PsmChargerType type = PsmChargerType_Unconnected;
    Result rc = psmGetChargerType(&type);
    int chg = -1;

    if(R_SUCCEEDED(rc)) {
        chg = (type == PsmChargerType_EnoughPower || type == PsmChargerType_LowPower) ? 1 : 0;
    }

    return chg;
}

int WifiSignal()
{
    NifmInternetConnectionType type = (NifmInternetConnectionType)0;
    u32 obars = 0;
    NifmInternetConnectionStatus status = (NifmInternetConnectionStatus)0;
    Result rc = nifmGetInternetConnectionStatus(&type, &obars, &status);

    int bars = -1;

    if (R_SUCCEEDED(rc)) {
        if (status == NifmInternetConnectionStatus_Connected &&
            type == NifmInternetConnectionType_WiFi) {
            bars = (obars > 3) ? 3 : (int)obars;
        } else {
            bars = 0;
        }
    }

    return bars;
}

int AirplaneMode()
{
    bool enabled = true;
    if (R_FAILED(nifmIsWirelessCommunicationEnabled(&enabled)))
        return -1;
    return enabled ? 0 : 1;
}

int ConnectionType()
{
    NifmInternetConnectionType type = (NifmInternetConnectionType)0;
    u32 bars = 0;
    NifmInternetConnectionStatus status = (NifmInternetConnectionStatus)0;
    Result rc = nifmGetInternetConnectionStatus(&type, &bars, &status);

    int conn = -1;

    if (R_SUCCEEDED(rc)) {
        if (status != NifmInternetConnectionStatus_Connected)
            conn = 0;
        else if (type == NifmInternetConnectionType_Ethernet)
            conn = 2;
        else if (type == NifmInternetConnectionType_WiFi)
            conn = 1;
        else
            conn = 0;
    }


    return conn;
}

ColorSetId ColorSet()
{
    ColorSetId cs = ColorSetId_Light;
    setsysGetColorSetId(&cs);
    return cs;
}

int AccountUidValid(AccountUid uid)
{
    return accountUidIsValid(&uid) ? 1 : 0;
}

} // namespace sysinfo
