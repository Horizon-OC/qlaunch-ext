/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "app.hpp"
#include <cstring>
#include <shared/logging.hpp>

namespace app {

namespace {

AppletApplication g_holder{};
u64 g_lastId = 0;

struct NacpMisc {
    u64 save_owner = 0;
    u64 account_size = 0;
    u64 account_journal = 0;
    u64 device_size = 0;
    u64 device_journal = 0;
    u64 temp_size = 0;
    u64 cache_size = 0;
    u64 cache_journal = 0;
    u64 bcat_size = 0;
};

/* NsApplicationControlData is NACP + 128KB icon */
NsApplicationControlData g_ctrl;

bool QueryNacpMisc(u64 app_id, NacpMisc &out)
{
    u64 actual = 0;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage,
                                            app_id, &g_ctrl, sizeof(g_ctrl),
                                            &actual);
    if (R_FAILED(rc) || actual < sizeof(NacpStruct))
        return false;
    const NacpStruct &n = g_ctrl.nacp;
    out.save_owner = n.save_data_owner_id;
    out.account_size = n.user_account_save_data_size;
    out.account_journal = n.user_account_save_data_journal_size;
    out.device_size = n.device_save_data_size;
    out.device_journal = n.device_save_data_journal_size;
    out.temp_size = n.temporary_storage_size;
    out.cache_size = n.cache_storage_size;
    out.cache_journal = n.cache_storage_journal_size;
    out.bcat_size = n.bcat_delivery_cache_storage_size;
    return true;
}

void EnsureSaveData(u64 app_id, u64 owner_id, const AccountUid &user_id,
                    FsSaveDataType type, FsSaveDataSpaceId space_id,
                    u64 save_size, u64 journal_size)
{
    if (save_size == 0)
        return;
    FsSaveDataAttribute attr;
    memset(&attr, 0, sizeof(attr));
    attr.application_id = app_id;
    attr.uid = user_id;
    attr.system_save_data_id = 0;
    attr.save_data_type = (u8)type;
    attr.save_data_rank = FsSaveDataRank_Primary;
    attr.save_data_index = 0;
    FsSaveDataCreationInfo cr;
    memset(&cr, 0, sizeof(cr));
    cr.save_data_size = (s64)save_size;
    cr.journal_size = (s64)journal_size;
    cr.available_size = 0x4000; // fixed value on all qlaunch saves
    cr.owner_id = owner_id;
    cr.flags = 0;
    cr.save_data_space_id = (u8)space_id;
    FsSaveDataMetaInfo meta;
    memset(&meta, 0, sizeof(meta));
    meta.size = (type == FsSaveDataType_Bcat) ? 0u : 0x40060u;
    meta.type = (type == FsSaveDataType_Bcat) ? FsSaveDataMetaType_None
                                               : FsSaveDataMetaType_Thumbnail;
    /* qlaunch probes existence by trying to open it. */
    FsFileSystem dummy;
    if (R_SUCCEEDED(fsOpenSaveDataFileSystem(&dummy, space_id, &attr))) {
        fsFsClose(&dummy);
        return;
    }
    Result rc = fsCreateSaveDataFileSystem(&attr, &cr, &meta);
    if (R_FAILED(rc))
        logging::LogLine("[app] save create type=%d rc=0x%X", (int)type, rc);
}

} // namespace

bool g_hasFocus = false;

bool IsActive()
{
    if (!eventActive(&g_holder.StateChangedEvent))
        return false;
    if (!serviceIsActive(&g_holder.s))
        return false;
    return !appletApplicationCheckFinished(&g_holder);
}

Result Terminate()
{
    Result rc = appletApplicationTerminateAllLibraryApplets(&g_holder);
    if (R_FAILED(rc))
        logging::LogLine("[app] terminate applets rc=0x%X", rc);
    rc = appletApplicationRequestExit(&g_holder);
    if (R_FAILED(rc))
        logging::LogLine("[app] request exit rc=0x%X", rc);
    rc = eventWait(&g_holder.StateChangedEvent, 15000000000ULL);
    if (rc == KERNELRESULT(TimedOut)) {
        logging::LogLine("[app] exit timed out, forcing terminate");
        rc = appletApplicationTerminate(&g_holder);
        if (R_FAILED(rc))
            return rc;
    } else if (R_FAILED(rc)) {
        return rc;
    }
    u32 out = 0;
    serviceDispatchOut(&g_holder.s, 30, out);
    logging::LogLine("[app] terminated result=0x%X", out);
    appletApplicationClose(&g_holder);
    g_hasFocus = false;
    g_lastId = 0;
    return 0;
}

Result Start(u64 app_id, bool system, const AccountUid &user_id)
{
    appletApplicationClose(&g_holder);
    g_hasFocus = false;

    if (system) {
        Result rc = appletCreateSystemApplication(&g_holder, app_id);
        if (R_FAILED(rc)) {
            logging::LogLine("[app] create system %016llX rc=0x%X",
                             (unsigned long long)app_id, rc);
            return rc;
        }
    } else {
        NacpMisc misc;
        if (QueryNacpMisc(app_id, misc)) {
            nsTouchApplication(app_id);
            AccountUid empty{};
            EnsureSaveData(app_id, misc.save_owner, user_id,
                           FsSaveDataType_Account, FsSaveDataSpaceId_User,
                           misc.account_size, misc.account_journal);
            EnsureSaveData(app_id, misc.save_owner, empty,
                           FsSaveDataType_Device, FsSaveDataSpaceId_User,
                           misc.device_size, misc.device_journal);
            EnsureSaveData(app_id, misc.save_owner, empty,
                           FsSaveDataType_Temporary, FsSaveDataSpaceId_Temporary,
                           misc.temp_size, 0);
            EnsureSaveData(app_id, misc.save_owner, empty,
                           FsSaveDataType_Cache, FsSaveDataSpaceId_User,
                           misc.cache_size, misc.cache_journal);
            EnsureSaveData(app_id, 0x010000000000000CULL, empty,
                           FsSaveDataType_Bcat, FsSaveDataSpaceId_User,
                           misc.bcat_size, 0x200000);
        } else {
            logging::LogLine("[app] no control data for %016llX",
                             (unsigned long long)app_id);
        }
        Result rc = appletCreateApplication(&g_holder, app_id);
        if (R_FAILED(rc)) {
            logging::LogLine("[app] create %016llX rc=0x%X",
                             (unsigned long long)app_id, rc);
            return rc;
        }
    }

    if (accountUidIsValid(&user_id)) {
        SelectedUserArgument arg{};
        arg.magic = SelectedUserArgument::Magic;
        arg.is_selected = 1;
        arg.uid = user_id;
        Result rc = Send(&arg, sizeof(arg), AppletLaunchParameterKind_PreselectedUser);
        if (R_FAILED(rc))
            logging::LogLine("[app] preselected user ignored rc=0x%X", rc);
    }

    Result rc = appletUnlockForeground();
    if (R_FAILED(rc)) {
        logging::LogLine("[app] unlock fg rc=0x%X", rc);
        return rc;
    }
    rc = appletApplicationStart(&g_holder);
    if (R_FAILED(rc)) {
        logging::LogLine("[app] start rc=0x%X", rc);
        appletApplicationClose(&g_holder);
        return rc;
    }
    rc = SetForeground();
    if (R_FAILED(rc)) {
        logging::LogLine("[app] fg rc=0x%X, terminating", rc);
        appletApplicationTerminate(&g_holder);
        appletApplicationJoin(&g_holder);
        appletApplicationClose(&g_holder);
        return rc;
    }
    g_lastId = app_id;
    logging::LogLine("[app] foreground %016llX", (unsigned long long)app_id);
    return 0;
}

bool HasForeground()
{
    return g_hasFocus;
}

Result SetForeground()
{
    Result rc = appletApplicationRequestForApplicationToGetForeground(&g_holder);
    if (R_FAILED(rc)) {
        logging::LogLine("[app] request fg rc=0x%X", rc);
        return rc;
    }
    g_hasFocus = true;
    return 0;
}

Result Send(const void *data, size_t size, AppletLaunchParameterKind kind)
{
    AppletStorage st{};
    Result rc = appletCreateStorage(&st, (s64)size);
    if (R_FAILED(rc))
        return rc;
    rc = appletStorageWrite(&st, 0, data, size);
    if (R_FAILED(rc)) {
        appletStorageClose(&st);
        return rc;
    }

    rc = appletApplicationPushLaunchParameter(&g_holder, kind, &st);
    if (R_FAILED(rc))
        appletStorageClose(&st);
    return rc;
}

u64 GetId()
{
    return g_lastId;
}

} // namespace app
