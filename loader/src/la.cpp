/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "la.hpp"
#include <cstring>
#include <shared/logging.hpp>

namespace la {

namespace {

AppletHolder g_holder{};

u32 ControllerVersion()
{
    if (hosversionAtLeast(11, 0, 0))
        return 0x8;
    if (hosversionAtLeast(8, 0, 0))
        return 0x7;
    if (hosversionAtLeast(6, 0, 0))
        return 0x5;
    if (hosversionAtLeast(3, 0, 0))
        return 0x4;
    return 0x3;
}

u32 ControllerArgSize()
{
    if (hosversionBefore(8, 0, 0))
        return sizeof(HidLaControllerSupportArgV3);
    return sizeof(HidLaControllerSupportArg);
}

Result Create(AppletId id, s32 version)
{
    if (IsActive()) {
        Result trc = Terminate();
        if (R_FAILED(trc))
            return trc;
    }
    Result rc = appletCreateLibraryApplet(&g_holder, id, LibAppletMode_AllForeground);
    if (R_FAILED(rc)) {
        logging::LogLine("[la] create %u rc=0x%X", (unsigned)id, rc);
        return rc;
    }

    if (version >= 0) {
        LibAppletArgs args;
        libappletArgsCreate(&args, (u32)version);
        libappletArgsSetPlayStartupSound(&args, true);
        rc = libappletArgsPush(&args, &g_holder);
        if (R_FAILED(rc)) {
            logging::LogLine("[la] args rc=0x%X", rc);
            appletHolderClose(&g_holder);
            return rc;
        }
    }
    return 0;
}

Result Launch()
{
    Result rc = appletHolderStart(&g_holder);
    if (R_FAILED(rc)) {
        logging::LogLine("[la] start rc=0x%X", rc);
        appletHolderClose(&g_holder);
        return rc;
    }
    return 0;
}

Result Send(const void *data, size_t size)
{
    return libappletPushInData(&g_holder, data, size);
}

} // namespace

bool IsActive()
{
    if (g_holder.StateChangedEvent.revent == INVALID_HANDLE)
        return false;
    if (!serviceIsActive(&g_holder.s))
        return false;
    return !appletHolderCheckFinished(&g_holder);
}

Result Terminate()
{
    /* Same 15s grace as uLaunch. */
    Result rc = appletHolderRequestExitOrTerminate(&g_holder, 15000000000ULL);
    if (R_FAILED(rc))
        logging::LogLine("[la] exit-or-terminate rc=0x%X", rc);
    appletHolderClose(&g_holder);
    return 0;
}

Result Start(AppletId id, s32 version)
{
    Result rc = Create(id, version);
    if (R_FAILED(rc))
        return rc;
    return Launch();
}

Result Start(AppletId id, s32 version, const void *a, size_t na)
{
    Result rc = Create(id, version);
    if (R_FAILED(rc))
        return rc;
    if (a && na) {
        rc = Send(a, na);
        if (R_FAILED(rc)) {
            appletHolderClose(&g_holder);
            return rc;
        }
    }
    return Launch();
}

Result Start(AppletId id, s32 version,
             const void *a, size_t na, const void *b, size_t nb)
{
    Result rc = Create(id, version);
    if (R_FAILED(rc))
        return rc;
    if (a && na) {
        rc = Send(a, na);
        if (R_FAILED(rc)) {
            appletHolderClose(&g_holder);
            return rc;
        }
    }
    if (b && nb) {
        rc = Send(b, nb);
        if (R_FAILED(rc)) {
            appletHolderClose(&g_holder);
            return rc;
        }
    }
    return Launch();
}

Result OpenAlbum()
{
    u8 arg = AlbumLaArg_ShowAllAlbumFilesForHomeMenu;
    logging::LogLine("[la] opening album");
    return Start(AppletId_LibraryAppletPhotoViewer, 0x10000, &arg, sizeof(arg));
}

Result OpenControllers()
{
    /* TODO: Reimplement controller applet */
    HidLaControllerSupportArgPrivate priv{};
    priv.private_size = sizeof(priv);
    priv.arg_size = ControllerArgSize();
    priv.flag0 = 0;
    priv.flag1 = 1;
    priv.mode = HidLaControllerSupportMode_ShowControllerSupport;
    priv.controller_support_caller = 0;
    if (R_FAILED(hidGetSupportedNpadStyleSet(&priv.npad_style_set)))
        priv.npad_style_set = 0;
    HidNpadJoyHoldType hold = HidNpadJoyHoldType_Horizontal;
    if (R_SUCCEEDED(hidGetNpadJoyHoldType(&hold)))
        priv.npad_joy_hold_type = hold;
    else
        priv.npad_joy_hold_type = HidNpadJoyHoldType_Horizontal;
    HidLaControllerSupportArg pub{};
    hidLaCreateControllerSupportArg(&pub);
    logging::LogLine("[la] opening controllers");
    return Start(AppletId_LibraryAppletController, (s32)ControllerVersion(),
                 &priv, sizeof(priv), &pub, ControllerArgSize());
}

Result OpenMiiEdit()
{
    /* TODO: Reimplement mii editor */
    s32 ver = hosversionAtLeast(10, 2, 0) ? 0x4 : 0x3;
    MiiLaAppletInput in{};
    in.version = ver;
    in.mode = MiiLaAppletMode_ShowMiiEdit;
    in.special_key_code = MiiSpecialKeyCode_Normal;
    logging::LogLine("[la] opening mii edit");
    return Start(AppletId_LibraryAppletMiiEdit, -1, &in, sizeof(in));
}

Result OpenHomeApplet(HomeApplet which)
{
    switch (which) {
    case HomeApplet::Album:
        return OpenAlbum();
    case HomeApplet::Controllers:
        return OpenControllers();
    case HomeApplet::MiiEdit:
        return OpenMiiEdit();
    default:
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }
}

} // namespace la
