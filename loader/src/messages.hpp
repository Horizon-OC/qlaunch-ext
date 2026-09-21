/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

namespace msg {

enum class AppletMessage : u32 {
    None = 0,
    ChangeIntoForeground = 1,
    ChangeIntoBackground = 2,
    ExitRequest = 4,
    ApplicationExited = 6,
    FocusStateChanged = 15,
    Resume = 16,
    DetectShortPressingHomeButton = 20,
    DetectLongPressingHomeButton = 21,
    DetectShortPressingPowerButton = 22,
    DetectMiddlePressingPowerButton = 23,
    DetectLongPressingPowerButton = 24,
    RequestToPrepareSleep = 25,
    FinishedSleepSequence = 26,
    SleepRequiredByHighTemperature = 27,
    SleepRequiredByLowBattery = 28,
    AutoPowerDown = 29,
    OperationModeChanged = 30,
    PerformanceModeChanged = 31,
    DetectReceivingCecSystemStandby = 32,
    SdCardRemoved = 33,
    LaunchApplicationRequested = 34,
    RequestToDisplay = 35,
    ShowApplicationLogo = 55,
    HideApplicationLogo = 56,
    ForceHideApplicationLogo = 57,
    FloatingApplicationDetected = 60,
    DetectShortPressingCaptureButton = 90,
    AlbumScreenShotTaken = 92,
    AlbumRecordingSaved = 93,
};

enum class GeneralChannelMessage : u32 {
    Invalid = 0,
    RequestHomeMenu = 2,
    Sleep = 3,
    Shutdown = 5,
    Reboot = 6,
    RequestJumpToSystemUpdate = 11,
};

struct SystemAppletMessageHeader {
    static constexpr u32 Magic = 0x534D4153; // "SMAS"
    u32 magic;
    u32 unk;
    GeneralChannelMessage msg;
    u32 unk_2;

    bool IsValid() const { return magic == Magic; }
};
static_assert(sizeof(SystemAppletMessageHeader) == 0x10);

} // namespace msg
