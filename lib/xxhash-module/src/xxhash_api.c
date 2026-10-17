/* qlaunch-ext xxhash-module (C) 2026 Souldbminer */
/* Licensed under the GPLv2                 */

#define XXH_INLINE_ALL
#include <xxhash.h>
#include <stddef.h>
#include <stdint.h>

/// @brief Hash a block with XXH64.
/// @param data Bytes to hash (null yields 0).
/// @param len Byte count.
/// @param seed Seed.
/// @return 64-bit hash.
uint64_t xxh64(const void *data, size_t len, uint64_t seed)
{
    if (data == NULL)
        return 0;
    return XXH64(data, len, (XXH64_hash_t)seed);
}

/// @brief Hash a block with XXH32.
/// @param data Bytes to hash (null yields 0).
/// @param len Byte count.
/// @param seed Seed.
/// @return 32-bit hash.
uint32_t xxh32(const void *data, size_t len, uint32_t seed)
{
    if (data == NULL)
        return 0;
    return XXH32(data, len, (XXH32_hash_t)seed);
}
