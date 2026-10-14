/* qlaunch-ext NRO SDK (C) 2026 Souldbminer */
/* Licensed under the GPLv2                */
/* Lets a plugin boot a nro under a donor title. */

#pragma once

#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

/* "QLDI". */
#define QEXT_NRO_MAGIC 0x49444C51u

enum {
    kNroPathSize = 512,
    kNroArgvSize = 2048,
};

/* Payload boot block. argv empty = defaults to nro_path. */
typedef struct {
    u32 magic;
    u8 _pad[4];
    char nro_path[kNroPathSize];
    char nro_argv[kNroArgvSize];
} NroTargetInput;

#ifdef __cplusplus
} /* extern C */

namespace nro {

/// @brief Stage the NRO payload over a donor for the next launch.
/// Writes contents/<donor>/exefs.nsp plus a marker; no-op when the
/// redirect for this donor is already staged.
/// @param donor Donor (real) title id.
/// @return True when the redirect is in place.
bool StageDonor(u64 donor);

/// @brief Delete the staged redirect, if any. Cheap when idle.
void Disarm();

/// @brief Boot an NRO under a donor title. Terminates any running
/// app first (same as real titles), stages the payload, pushes the
/// NRO path, and starts the donor.
/// @param donor Donor (real) title id backing the launch.
/// @param nro_path NRO path (sdmc:/...). Must be non-empty.
/// @param argv_or_null argv for the NRO (NULL/empty = nro_path).
/// @return 0 when the donor start was issued.
Result Launch(u64 donor, const char *nro_path, const char *argv_or_null);

/// @brief Delete crash leftovers (loader-internal; runs on boot).
/// Only dirs carrying our marker are touched.
void PruneStale();

} // namespace nro
#endif
