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

        /// @brief Quesry the misc NACP parts
        /// @param app_id The ID for the app
        /// @param out The output struct
        /// @return True/false
        bool QueryNacpMisc(u64 app_id, NacpMisc &out) {
            u64 nacpMiscSz = 0;
            Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, app_id, &g_ctrl, sizeof(g_ctrl), &nacpMiscSz);

            if (R_FAILED(rc) || nacpMiscSz < sizeof(NacpStruct)) {
                return false;
            }

            // Create the struct with the new parameters
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

        /// @brief Ensure the app has save data
        /// @param app_id The app ID
        /// @param owner_id The ownder ID
        /// @param user_id The account's UID
        /// @param type The save data type
        /// @param space_id Where to put the save
        /// @param save_size Size of the save
        /// @param journal_size Journaling size
        void EnsureSaveData(u64 app_id, u64 owner_id, const AccountUid &user_id, FsSaveDataType type, FsSaveDataSpaceId space_id, u64 save_size, u64 journal_size) {
            if (save_size == 0) {
                return;
            }
            /* Set the save data attribute */
            FsSaveDataAttribute attr;
            memset(&attr, 0, sizeof(attr));

            attr.application_id = app_id;
            attr.uid = user_id;
            attr.system_save_data_id = 0;
            attr.save_data_type = (u8)type;
            attr.save_data_rank = FsSaveDataRank_Primary;
            attr.save_data_index = 0;

            /* Set the save data creation info */
            FsSaveDataCreationInfo cr;
            memset(&cr, 0, sizeof(cr));

            cr.save_data_size = (s64)save_size;
            cr.journal_size = (s64)journal_size;
            cr.available_size = 0x4000; // fixed value on all qlaunch saves
            cr.owner_id = owner_id;
            cr.flags = 0;
            cr.save_data_space_id = (u8)space_id;

            /* Set the save data metadata*/
            FsSaveDataMetaInfo meta;
            memset(&meta, 0, sizeof(meta));

            meta.size = (type == FsSaveDataType_Bcat) ? 0u : 0x40060u;
            meta.type = (type == FsSaveDataType_Bcat) ? FsSaveDataMetaType_None
                                                    : FsSaveDataMetaType_Thumbnail;
            /* Open the filesystem for the space*/
            FsFileSystem dummy;
            if (R_SUCCEEDED(fsOpenSaveDataFileSystem(&dummy, space_id, &attr))) {
                fsFsClose(&dummy);
                return;
            }

            /* Create the filesystem */
            Result rc = fsCreateSaveDataFileSystem(&attr, &cr, &meta);
            if (R_FAILED(rc)) {
                logging::LogLine("[app] Save created (type %d, rc=0x%X)", (int)type, rc);
            }
        }

    } // namespace

    bool g_hasFocus = false;

    /// @brief Is the state changed event/service active 
    /// @return True/false
    bool IsActive() {
        if (!eventActive(&g_holder.StateChangedEvent)) {
            return false;
        }

        if (!serviceIsActive(&g_holder.s)) {
            return false;
        }

        return !appletApplicationCheckFinished(&g_holder);
    }

    /// @brief Terminate the currently running app
    /// @return Result code
    Result Terminate() {
        Result rc = appletApplicationTerminateAllLibraryApplets(&g_holder);
        if (R_FAILED(rc)) {
            logging::LogLine("[app] Cant terminate applets (rc=0x%X)", rc);
        }

        rc = appletApplicationRequestExit(&g_holder);
        if (R_FAILED(rc)) {
            logging::LogLine("[app] Can't request applet exit (rc=0x%X)", rc);
        }

        rc = eventWait(&g_holder.StateChangedEvent, 15000000000ULL);
        if (rc == KERNELRESULT(TimedOut)) {
            logging::LogLine("[app] Exit timeout, forcefully terminating");

            rc = appletApplicationTerminate(&g_holder);
            if (R_FAILED(rc)) {
                return rc;
            }
        } else if (R_FAILED(rc)) {
            return rc;
        }

        u32 out = 0;
        serviceDispatchOut(&g_holder.s, 30, out);

        logging::LogLine("[app] App terminated (rc=0x%X)", out);
        appletApplicationClose(&g_holder);

        g_hasFocus = false;
        g_lastId = 0;
        return 0;
    }

    /// @brief Start a app
    /// @param app_id The app ID
    /// @param system Is it a system app
    /// @param user_id What user to provide to the app
    /// @return Result code
    Result Start(u64 app_id, bool system, const AccountUid &user_id) {
        appletApplicationClose(&g_holder);
        g_hasFocus = false;
        /* Create the system application if nessesary */
        if (system) {
            Result rc = appletCreateSystemApplication(&g_holder, app_id);
            if (R_FAILED(rc)) {
                logging::LogLine("[app] create system %016llX rc=0x%X",
                                (unsigned long long)app_id, rc);
                return rc;
            }
        } else {
            NacpMisc misc;
            /* Ensure the saves are properly set up */
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
                /* Should not happen */
                logging::LogLine("[app] no control data for %016llX", (unsigned long long)app_id);
            }
            /* Actually create the app*/
            Result rc = appletCreateApplication(&g_holder, app_id);
            if (R_FAILED(rc)) {
                logging::LogLine("[app] Created %016llX (rc=0x%X)", (unsigned long long)app_id, rc);
                return rc;
            }
        }

        /* Select the user */
        if (accountUidIsValid(&user_id)) {
            SelectedUserArgument arg{};
            arg.magic = SelectedUserArgument::Magic;
            arg.is_selected = 1;
            arg.uid = user_id;
            Result rc = Send(&arg, sizeof(arg), AppletLaunchParameterKind_PreselectedUser);
            if (R_FAILED(rc))
                logging::LogLine("[app] Ignoring preselected user (rc=0x%X)", rc);
        }
        /* Unlock the foreground */
        Result rc = appletUnlockForeground();
        if (R_FAILED(rc)) {
            logging::LogLine("[app] Unlock foreground (rc=0x%X)", rc);
            return rc;
        }

        /* Start the application*/
        rc = appletApplicationStart(&g_holder);
        if (R_FAILED(rc)) {
            logging::LogLine("[app] Cant start (rc=0x%X)", rc);
            appletApplicationClose(&g_holder);
            return rc;
        }
        /* Give the app foreground */
        rc = SetForeground();
        if (R_FAILED(rc)) {
            logging::LogLine("[app] Setting foreground failed, rc=0x%X, terminating", rc);
            appletApplicationTerminate(&g_holder);
            appletApplicationJoin(&g_holder);
            appletApplicationClose(&g_holder);
            return rc;
        }

        /* Logging */
        g_lastId = app_id;
        logging::LogLine("[app] Started and foregrounded %016llX", (unsigned long long)app_id);
        return 0;
    }

    /// @brief Is the app in foreground
    /// @return True/false
    bool HasForeground() {
        return g_hasFocus;
    }
    
    /// @brief Give a app foreground
    /// @return Result code
    Result SetForeground() {
        Result rc = appletApplicationRequestForApplicationToGetForeground(&g_holder);
        if (R_FAILED(rc)) {
            logging::LogLine("[app] Requesting foreground (rc=0x%X)", rc);
            return rc;
        }
        g_hasFocus = true;
        return 0;
    }

    /// @brief 
    /// @param data 
    /// @param size 
    /// @param kind 
    /// @return 
    Result Send(const void *data, size_t size, AppletLaunchParameterKind kind) {
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

    /// @brief 
    /// @return 
    u64 GetId() {
        return g_lastId;
    }

} // namespace app
