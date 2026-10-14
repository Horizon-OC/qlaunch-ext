/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

/* TitlePlugin interface: virtual titles injected into the menu.
 *
 * A plugin becomes a title provider by exporting:
 *
 *   int TitleCount(void);
 *   u64 TitleTid(int index);                        (optional)
 *   u64 TitleDonor(int index);                      (optional)
 *   int TitleNro(int index, char *out, unsigned cap);   (optional)
 *   int TitleName(int index, char *out, unsigned cap);
 *   int TitleIconSize(int index);                       (optional)
 *   int TitleIcon(int index, void *out, unsigned cap);  (optional)
 *   int TitleLaunch(int index);             (optional with TitleNro)
 *   void TitleRefresh(void);                            (optional)
 *
 * TitleCount: number of virtual titles (0 = none for now).
 * TitleTid: stable fake tid for the title. Must have the high bit set
 *   (virtual space) and be unique across providers; when missing or
 *   unusable the loader assigns one from the provider slot instead.
 *   Prefer fixed fake tids (they keep order.json ranks and icon caches
 *   stable across reboots even if plugin load order changes).
 * TitleDonor: donor title id backing the launch (0 = none). The donor
 *   is only the application container (title id, save data); its own
 *   code never runs.
 * TitleNro: NRO path (sdmc:/...) booted under the donor through the
 *   loader's versioned NRO payload (see <qlaunch/nro.h>). When set
 *   (with a donor), tapping the title boots the NRO and TitleLaunch
 *   is neither required nor called. Without it the title is plain:
 *   TitleLaunch handles the tap (0 = handled).
 * TitleName: name length into out (<0 on error).
 * TitleIconSize/TitleIcon: JPEG bytes for the menu tile. JPEG only
 *   (the menu checks for SOI magic); <0 or missing means the menu
 *   draws its fallback tile. Max 256KB per icon.
 * TitleLaunch: called when the user launches a plain (NRO-less)
 *   title. 0 = handled. The loader terminates any running game
 *   first (same as real titles), then dispatches here. The loader
 *   never forwards virtual tids to Nintendo services, so virtual
 *   titles only ever exist menu-side.
 * TitleRefresh: re-enumerate hook, called on every titles refresh.
 *
 * Virtual tids look like 0x8xxx... (high bit set) and encode the
 * provider slot plus the item index. Real tids never have the high
 * bit set, so the two spaces cannot collide. Menu code needs no
 * changes: titles are index-addressed and launch is tid-addressed.
 *
 * Limits: 32 providers, 128 titles each, 256 virtual titles total.
 * Item order must be stable while loaded; icon caches key on tid.
 */

namespace vtitles {

    constexpr int kMaxProviders = 32;
    constexpr int kMaxPerProvider = 128;
    constexpr int kMaxTotal = 256;
    constexpr int kNameCap = 192;
    constexpr int kIconCap = 0x40000;

    /// @brief Probe a freshly loaded plugin handle for title exports.
    /// @param handle dlopen handle from the plugin loader.
    /// @return True when the plugin provides titles and a slot was free.
    bool Register(void *handle);

    /// @brief Release a provider slot before its plugin unloads.
    /// @param handle dlopen handle being closed.
    void Unregister(void *handle);

    /// @brief Re-query counts (and TitleRefresh hooks) of all providers.
    void Refresh();

    /// @brief Give the current virtual title count.
    /// @return Virtual title count.
    int Count();

    /// @brief Give the virtual tid for a flat virtual index.
    /// @param vi Flat index in [0, Count()).
    /// @return Virtual tid, or 0 when out of range.
    u64 Id(int vi);

    /// @brief Reverse-lookup a virtual tid to its flat index.
    /// @param tid Virtual title id.
    /// @return Flat index, or -1 when not a live virtual title.
    int IndexOf(u64 tid);

    /// @brief Read a virtual title's name.
    /// @param vi Flat virtual index.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Name length (<0 on error).
    int Name(int vi, char *out, unsigned cap);

    /// @brief Give a virtual title's JPEG icon size.
    /// @param vi Flat virtual index.
    /// @return Byte size (<0 if none).
    int IconSize(int vi);

    /// @brief Copy a virtual title's JPEG icon bytes.
    /// @param vi Flat virtual index.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Bytes written (<0 on error).
    int Icon(int vi, void *out, unsigned cap);

    /// @brief Test whether a tid addresses a virtual title.
    /// @param tid Title id to test.
    /// @return True for virtual tids.
    bool IsId(u64 tid);

    /// @brief Give the donor backing a virtual title, if any.
    /// @param tid Virtual title id.
    /// @return Donor title id, or 0 for plain virtual titles.
    u64 DonorOf(u64 tid);

    /// @brief Give the NRO booted under a virtual title's donor.
    /// @param tid Virtual title id.
    /// @param out Output buffer for the sdmc:/... path.
    /// @param cap Buffer capacity.
    /// @return Path length (<0 when the title is not NRO-backed).
    int NroOf(u64 tid, char *out, unsigned cap);

    /// @brief Dispatch a launch to the owning provider.
    /// @param tid Virtual title id.
    /// @return 0 when handled, libnx error otherwise.
    Result Launch(u64 tid);

    /// @brief Consume a pending virtual-set change notification.
    /// @return True once when providers or counts changed.
    bool Changed();

} // namespace vtitles
