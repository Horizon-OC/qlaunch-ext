/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "sysinfo.hpp"
#include <shared/logging.hpp>

namespace sysinfo {

    namespace {
        constexpr unsigned kImageCap = 0x40000;
    } // namespace

    int AccountList(AccountUid *out, int cap) {
        /* Validate input */
        if (!out || cap <= 0)
            return -1;

        s32 total = 0;

        /* List all users and return their UIDs */
        if (R_FAILED(accountListAllUsers(out, cap, &total)))
            return -1;
        
        return (int)total;
    }

    int AccountImageSize(AccountUid uid) {
        AccountProfile prof{};
        
        /* Get the profile from the UID */
        if (R_FAILED(accountGetProfile(&prof, uid)))
            return -1;
        
        u32 size = 0;
        /* Get the image from the profile we just got*/
        Result rc = accountProfileGetImageSize(&prof, &size);
        accountProfileClose(&prof);
        
        if (R_FAILED(rc) || size <= 2 || size > kImageCap)
            return -1;
        return (int)size;
    }

    int AccountImage(AccountUid uid, void *out, unsigned cap) {
        /* Validate input */
        int neededSize = AccountImageSize(uid);
        if (neededSize < 0 || !out || (unsigned)neededSize > cap)
            return -1;
        
        /* Get the profile */
        AccountProfile prof{};
        if (R_FAILED(accountGetProfile(&prof, uid)))
            return -1;
        
        u8 *jpg = (u8 *)out;
        u32 loaded = 0;
        /* Load the image and close the profile */
        Result rc = accountProfileLoadImage(&prof, jpg, (u32)neededSize, &loaded);
        accountProfileClose(&prof);

        if (R_FAILED(rc) || neededSize <= 2 || loaded > (u32)neededSize)
            return -1;
        return (int)loaded;
    }

    Result TimeNow(u64 *ts) {
        /* Validate timestamp pointer */
        if (!ts)
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        
        /* Get the current time into the timestamp */
        return timeGetCurrentTime(TimeType_UserSystemClock, ts);
    }

    Result TimeCalendar(u64 ts, TimeCalendarTime *cal, TimeCalendarAdditionalInfo *info) {
        /* Validate pointers */
        if (!cal || !info)
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);

        /* Convert the time and populate the structure */
        return timeToCalendarTimeWithMyRule(ts, cal, info);
    }

    int BatteryPercent() {
        u32 percentage = 0;
        /* Get the percentage and validate it */
        if (R_FAILED(psmGetBatteryChargePercentage(&percentage)) || percentage > 100)
            return -1;
        return (int)percentage;
    }

    int BatteryCharging() {
        /* Get the charger type and return if the charger is connected */
        PsmChargerType type = PsmChargerType_Unconnected;
        Result rc = psmGetChargerType(&type);
        int chg = -1;

        if(R_SUCCEEDED(rc)) {
            chg = (int)(type != PsmChargerType_Unconnected);
        }

        return chg;
    }

    int WifiSignal() {
        NifmInternetConnectionType type = (NifmInternetConnectionType)0;
        u32 outBars = 0;
        NifmInternetConnectionStatus status = (NifmInternetConnectionStatus)0;
        Result rc = nifmGetInternetConnectionStatus(&type, &outBars, &status);

        int bars = -1;

        if (R_SUCCEEDED(rc)) {
            if (status == NifmInternetConnectionStatus_Connected && type == NifmInternetConnectionType_WiFi) {
                bars = (outBars > 3) ? 3 : (int)outBars;
            } else {
                /* Could be Ethernet */
                bars = 0;
            }
        }

        return bars;
    }


    /* Airplane mode could be enabled even if BT is disabled, so we may have to check for that in future? */
    int AirplaneMode() {
        bool enabled = true;
        if (R_FAILED(nifmIsWirelessCommunicationEnabled(&enabled)))
            return -1;
        return enabled ? 0 : 1;
    }

    int ConnectionType() {
        /* Get all the required data */
        NifmInternetConnectionType type = (NifmInternetConnectionType)0;
        u32 bars = 0;
        NifmInternetConnectionStatus status = (NifmInternetConnectionStatus)0;
        Result rc = nifmGetInternetConnectionStatus(&type, &bars, &status);

        int connectionType = -1;
        /* Return the appropraite output*/
        if (R_SUCCEEDED(rc)) {
            if (status != NifmInternetConnectionStatus_Connected)
                connectionType = 0;
            else if (type == NifmInternetConnectionType_Ethernet)
                connectionType = 2;
            else if (type == NifmInternetConnectionType_WiFi)
                connectionType = 1;
            else
                connectionType = 0;
        }

        return connectionType;
    }

    ColorSetId ColorSet() {
        /* Get the Color Set ID */
        /* Menus shouldn't use this */
        ColorSetId cs = ColorSetId_Light;
        setsysGetColorSetId(&cs);
        return cs;
    }

    int AccountUidValid(AccountUid uid) {
        return accountUidIsValid(&uid) ? 1 : 0;
    }

} // namespace sysinfo
