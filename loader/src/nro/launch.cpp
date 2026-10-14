/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering...         */
/* NRO launch: stage our versioned payload over a donor title,
 * push the NRO target block, start the donor. Callable from any
 * plugin (same process) via <qlaunch/nro.h>. */

#include <qlaunch/nro.h>
#include "../app.hpp"
#include <shared/logging.hpp>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

namespace nro {

namespace {

/// @brief TODO: cleanup
constexpr char kContentsDir[] = "sdmc:/atmosphere/contents";

/// @brief NRO payload
constexpr char kPayloadNsp[] = "sdmc:/qlaunch-ext/bin/nroloader.nsp";

/// @brief Markers
constexpr char kMarker[] = ".qlaunch-nro";

/// @brief Donor with a staged redirect right now (0 = none).
u64 s_armed = 0;

/// @brief Crash-leftover prune already ran this boot.
bool s_pruned = false;

/// @brief Format a donor contents dir path.
/// @param donor Donor title id.
/// @param out Output buffer.
/// @param cap Buffer capacity.
/// @return True when the path fit.
bool DonorDir(u64 donor, char *out, unsigned cap)
{
    int w = snprintf(out, cap, "%s/%016llX", kContentsDir, (unsigned long long)donor);
    return w > 0 && (unsigned)w < cap;
}

/// @brief Remove one staged redirect (file + marker + dir).
/// @param dir Donor contents dir.
void RemoveStaged(const char *dir)
{
    char path[352];
    int w = snprintf(path, sizeof(path), "%s/exefs.nsp", dir);
    if (w > 0 && (unsigned)w < sizeof(path))
        remove(path);
    w = snprintf(path, sizeof(path), "%s/%s", dir, kMarker);
    if (w > 0 && (unsigned)w < sizeof(path))
        remove(path);
    rmdir(dir); /* succeeds only when empty; ignore errors */
}

/// @brief Pick the last-opened user for the launch parameter.
/// @return User uid, possibly invalid (launch still works).
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

bool StageDonor(u64 donor)
{
    if (donor == 0)
        return false;
    char dir[320];
    if (!DonorDir(donor, dir, sizeof(dir)))
        return false;
    char dst[352];
    int w = snprintf(dst, sizeof(dst), "%s/exefs.nsp", dir);
    if (w <= 0 || (unsigned)w >= sizeof(dst))
        return false;
    if (s_armed == donor) {
        FILE *have = fopen(dst, "rb");
        if (have) {
            fclose(have);
            return true;
        }
    } else if (s_armed != 0) {
        Disarm();
    }
    struct stat a{};
    if (stat(kPayloadNsp, &a) != 0 || a.st_size <= 0) {
        logging::LogLine("[nro] missing %s, cannot stage donor %016llX",
                         kPayloadNsp, (unsigned long long)donor);
        return false;
    }
    mkdir("sdmc:/atmosphere", 0777);
    mkdir(kContentsDir, 0777);
    mkdir(dir, 0777);
    FILE *in = fopen(kPayloadNsp, "rb");
    if (!in)
        return false;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        logging::LogLine("[nro] cannot write %s", dst);
        fclose(in);
        return false;
    }
    char chunk[8192];
    size_t n;
    bool ok = true;
    while ((n = fread(chunk, 1, sizeof(chunk), in)) > 0) {
        if (fwrite(chunk, 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    fclose(in);
    fclose(out);
    if (!ok) {
        logging::LogLine("[nro] short copy to %s", dst);
        return false;
    }
    char mark[384];
    w = snprintf(mark, sizeof(mark), "%s/%s", dir, kMarker);
    if (w > 0 && (unsigned)w < sizeof(mark)) {
        FILE *mf = fopen(mark, "wb");
        if (mf) {
            char body[64];
            int m = snprintf(body, sizeof(body), "qlaunch-ext nro donor %016llX\n",
                             (unsigned long long)donor);
            if (m > 0)
                fwrite(body, 1, (unsigned)m, mf);
            fclose(mf);
        }
    }
    s_armed = donor;
    logging::LogLine("[nro] staged donor %016llX", (unsigned long long)donor);
    return true;
}

void Disarm()
{
    if (s_armed == 0)
        return;
    char dir[320];
    if (DonorDir(s_armed, dir, sizeof(dir)))
        RemoveStaged(dir);
    logging::LogLine("[nro] disarmed donor %016llX", (unsigned long long)s_armed);
    s_armed = 0;
}

void PruneStale()
{
    if (s_pruned)
        return;
    s_pruned = true;
    DIR *d = opendir(kContentsDir);
    if (!d)
        return;
    for (struct dirent *e = readdir(d); e; e = readdir(d)) {
        if (e->d_name[0] == '.' || strlen(e->d_name) != 16)
            continue;
        char dir[320];
        int w = snprintf(dir, sizeof(dir), "%s/%s", kContentsDir, e->d_name);
        if (w <= 0 || (unsigned)w >= sizeof(dir))
            continue;
        char mark[352];
        w = snprintf(mark, sizeof(mark), "%s/%s", dir, kMarker);
        if (w <= 0 || (unsigned)w >= sizeof(mark))
            continue;
        FILE *f = fopen(mark, "rb");
        if (!f)
            continue;
        fclose(f);
        RemoveStaged(dir);
        logging::LogLine("[nro] pruned stale redirect %s", dir);
    }
    closedir(d);
}

Result Launch(u64 donor, const char *nro_path, const char *argv_or_null)
{
    if (donor == 0 || nro_path == nullptr || nro_path[0] == '\0')
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    if (app::IsActive()) {
        logging::LogLine("[nro] replacing running %016llX",
                         (unsigned long long)app::GetId());
        Result rc = app::Terminate();
        if (R_FAILED(rc))
            return rc;
    }
    if (!StageDonor(donor))
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    NroTargetInput in{};
    in.magic = QEXT_NRO_MAGIC;
    size_t n = strlen(nro_path);

    if (n >= kNroPathSize)
        n = kNroPathSize - 1;

    memcpy(in.nro_path, nro_path, n);
    in.nro_path[n] = '\0';
    const char *argv = (argv_or_null != nullptr && argv_or_null[0] != '\0') ? argv_or_null : nro_path;
    
    n = strlen(argv);
    if (n >= kNroArgvSize)
        n = kNroArgvSize - 1;

    memcpy(in.nro_argv, argv, n);
    in.nro_argv[n] = '\0';
    logging::LogLine("[nro] booting '%s' with donor title %016llX", in.nro_path, (unsigned long long)donor);
            
    return app::Start(donor, false, PickUser(), &in, sizeof(in));
}

} // namespace nro
