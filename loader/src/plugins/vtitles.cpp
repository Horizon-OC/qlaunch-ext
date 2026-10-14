/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "vtitles.hpp"
#include <dlink/dlink.h>
#include <qlaunch/nro.h>
#include <cstring>
#include <shared/logging.hpp>

namespace vtitles {

    namespace {

        /// @brief High bit marks the virtual tid space
        constexpr u64 kVirtualBit = 0x8000000000000000ULL;

        /// @brief Provider slot
        struct Provider {
            bool used = false;
            void *handle = nullptr;
            int (*count)(void) = nullptr;
            u64 (*tid)(int) = nullptr;
            u64 (*donor)(int) = nullptr;
            int (*nro)(int, char *, unsigned) = nullptr;
            int (*name)(int, char *, unsigned) = nullptr;
            int (*iconSize)(int) = nullptr;
            int (*icon)(int, void *, unsigned) = nullptr;
            int (*launch)(int) = nullptr;
            void (*refresh)(void) = nullptr;
            int cached = 0;
        };

        /// @brief Virtual index entry
        struct Entry {
            int provider = -1;
            int index = -1;
            u64 vtid = 0;
        };

        Provider s_providers[kMaxProviders];
        Entry s_entries[kMaxTotal];
        int s_count = 0;
        bool s_changed = false;

        /// @brief Look up an export on a plugin handle.
        /// @param h dlopen handle.
        /// @param sym Symbol name.
        /// @return Symbol address, or null when missing.
        void *FindSymbol(void *h, const char *sym) {
            return dlsym(h, sym);
        }

        /// @brief Decode a virtual tid into provider slot + item index.
        /// @param tid Virtual title id.
        /// @param slot Output provider slot.
        /// @param index Output item index.
        /// @return True when well-formed and owned by a live provider.
        bool DecodeTid(u64 tid, int &slot, int &index) {
            slot = -1;
            index = -1;
            if ((tid & kVirtualBit) == 0)
                return false;

            /* Tids are registry-assigned (provider-chosen or fallback),
               so resolve through the flat table, not the bits. */
            for (int vi = 0; vi < s_count; vi++) {
                if (s_entries[vi].vtid != tid)
                    continue;

                const Entry &entry = s_entries[vi];
                if (entry.provider < 0 || entry.provider >= kMaxProviders || entry.index < 0)
                    continue;

                if (!s_providers[entry.provider].used ||
                    (!s_providers[entry.provider].launch && !s_providers[entry.provider].nro))
                    continue;

                if (entry.index >= s_providers[entry.provider].cached)
                    continue;

                slot = entry.provider;
                index = entry.index;
                return true;
            }

            return false;
        }

        /// @brief Check whether a tid is already assigned this rebuild.
        /// Self-assignment (same provider+index in the previous table)
        /// does not count, so stable provider tids survive recounts.
        /// @param tid Candidate virtual tid.
        /// @param requestSlot Requesting provider slot.
        /// @param requestIndex Requesting item index.
        /// @param previous Previous flat table.
        /// @param previousTotal Previous entry count.
        /// @param assigned Entries assigned so far (s_entries prefix).
        /// @param assignedTotal Their count.
        /// @return True when taken (or not in the virtual space).
        bool IsTaken(u64 tid, int requestSlot, int requestIndex, const Entry *previous, int previousTotal,
                     const Entry *assigned, int assignedTotal) {
            if ((tid & kVirtualBit) == 0 || tid == kVirtualBit)
                return true;

            for (int vi = 0; vi < previousTotal; vi++) {
                if (previous[vi].vtid == tid &&
                    (previous[vi].provider != requestSlot || previous[vi].index != requestIndex))
                    return true;
            }

            for (int vi = 0; vi < assignedTotal; vi++) {
                if (assigned[vi].vtid == tid)
                    return true;
            }

            return false;
        }

        /// @brief Rebuild the flat index from provider counts.
        void RebuildIndex() {
            Entry previous[kMaxTotal];
            int previousTotal = s_count;
            for (int vi = 0; vi < previousTotal; vi++)
                previous[vi] = s_entries[vi];

            int total = 0;
            for (int slot = 0; slot < kMaxProviders && total < kMaxTotal; slot++) {
                if (!s_providers[slot].used || !s_providers[slot].count)
                    continue;

                int titleCount = s_providers[slot].count();
                if (titleCount < 0)
                    titleCount = 0;
                if (titleCount > kMaxPerProvider)
                    titleCount = kMaxPerProvider;
                s_providers[slot].cached = titleCount;

                for (int index = 0; index < titleCount && total < kMaxTotal; index++) {
                    u64 vtid = 0;
                    if (s_providers[slot].tid) {
                        u64 want = s_providers[slot].tid(index);
                        if (!IsTaken(want, slot, index, previous, previousTotal, s_entries, total))
                            vtid = want;
                    }

                    if (vtid == 0) {
                        /* Biased by one so slot 0 item 0 never collapses to
                           the bare virtual bit (which IsTaken rejects). */
                        u64 fallback = kVirtualBit | ((u64)(u32)(slot + 1) << 32) |
                                       (u64)(u32)(index + 1);
                        if (!IsTaken(fallback, slot, index, previous, previousTotal, s_entries, total))
                            vtid = fallback;
                    }

                    if (vtid == 0)
                        continue; /* no unique tid available; skip */

                    s_entries[total].provider = slot;
                    s_entries[total].index = index;
                    s_entries[total].vtid = vtid;
                    total++;
                }
            }

            if (total != s_count)
                s_changed = true;
            s_count = total;

            /* A reshuffled tid set must also notify, even at same size. */
            if (s_count == previousTotal) {
                for (int vi = 0; vi < s_count; vi++) {
                    if (s_entries[vi].vtid != previous[vi].vtid ||
                        s_entries[vi].provider != previous[vi].provider ||
                        s_entries[vi].index != previous[vi].index) {
                        s_changed = true;
                        break;
                    }
                }
            }
        }

    } // namespace

    /// @brief Probe a freshly loaded plugin handle for title exports.
    /// @param handle dlopen handle from the plugin loader.
    /// @return True when the plugin provides titles and a slot was free.
    bool Register(void *handle) {
        if (!handle)
            return false;

        auto count = (int (*)(void))FindSymbol(handle, "TitleCount");
        auto name = (int (*)(int, char *, unsigned))FindSymbol(handle, "TitleName");
        auto launch = (int (*)(int))FindSymbol(handle, "TitleLaunch");
        auto nro = (int (*)(int, char *, unsigned))FindSymbol(handle, "TitleNro");
        if (!count || !name || (!launch && !nro))
            return false;

        for (int slot = 0; slot < kMaxProviders; slot++) {
            if (s_providers[slot].used)
                continue;

            if (s_providers[slot].handle == handle)
                return true;

            Provider provider{};
            provider.used = true;
            provider.handle = handle;
            provider.count = count;
            provider.tid = (u64 (*)(int))FindSymbol(handle, "TitleTid");
            provider.donor = (u64 (*)(int))FindSymbol(handle, "TitleDonor");
            provider.nro = nro;
            provider.name = name;
            provider.iconSize = (int (*)(int))FindSymbol(handle, "TitleIconSize");
            provider.icon = (int (*)(int, void *, unsigned))FindSymbol(handle, "TitleIcon");
            provider.launch = launch;
            provider.refresh = (void (*)(void))FindSymbol(handle, "TitleRefresh");
            s_providers[slot] = provider;

            RebuildIndex();
            return true;
        }

        return false;
    }

    /// @brief Release a provider slot before its plugin unloads.
    /// @param handle dlopen handle being closed.
    void Unregister(void *handle) {
        if (!handle)
            return;

        for (int slot = 0; slot < kMaxProviders; slot++) {
            if (s_providers[slot].used && s_providers[slot].handle == handle) {
                nro::Disarm();
                s_providers[slot] = Provider{};
                RebuildIndex();

                return;
            }
        }
    }

    /// @brief Re-query counts (and TitleRefresh hooks) of all providers.
    void Refresh() {
        nro::PruneStale();

        for (int slot = 0; slot < kMaxProviders; slot++) {
            if (s_providers[slot].used && s_providers[slot].refresh)
                s_providers[slot].refresh();
        }

        RebuildIndex();
    }

    /// @brief Give the current virtual title count.
    /// @return Virtual title count.
    int Count() {
        return s_count;
    }

    /// @brief Give the virtual tid for a flat virtual index.
    /// @param vi Flat index in [0, Count()).
    /// @return Virtual tid, or 0 when out of range.
    u64 Id(int vi) {
        if (vi < 0 || vi >= s_count)
            return 0;
        return s_entries[vi].vtid;
    }

    /// @brief Read a virtual title's name.
    /// @param vi Flat virtual index.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Name length (<0 on error).
    int Name(int vi, char *out, unsigned cap) {
        if (vi < 0 || vi >= s_count || !out || !cap)
            return -1;

        int slot = s_entries[vi].provider;
        int index = s_entries[vi].index;

        if (!s_providers[slot].used || !s_providers[slot].name)
            return -1;

        int len = s_providers[slot].name(index, out, cap);
        if (len < 0 || out[0] == 0)
            return -1;

        out[cap - 1] = 0;
        return (int)strlen(out);
    }

    /// @brief Give a virtual title's JPEG icon size.
    /// @param vi Flat virtual index.
    /// @return Byte size (<0 if none).
    int IconSize(int vi) {
        if (vi < 0 || vi >= s_count)
            return -1;

        int slot = s_entries[vi].provider;
        int index = s_entries[vi].index;

        if (!s_providers[slot].used || !s_providers[slot].iconSize)
            return -1;

        int size = s_providers[slot].iconSize(index);
        if (size <= 2 || size > kIconCap)
            return -1;
        return size;
    }

    /// @brief Copy a virtual title's JPEG icon bytes.
    /// @param vi Flat virtual index.
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Bytes written (<0 on error).
    int Icon(int vi, void *out, unsigned cap) {
        if (vi < 0 || vi >= s_count || !out || !cap)
            return -1;

        int slot = s_entries[vi].provider;
        int index = s_entries[vi].index;

        if (!s_providers[slot].used || !s_providers[slot].icon)
            return -1;

        int want = IconSize(vi);
        if (want < 0 || (unsigned)want > cap)
            return -1;

        int got = s_providers[slot].icon(index, out, (unsigned)want);
        if (got <= 2 || got > want)
            return -1;
        return got;
    }

    /// @brief Reverse-lookup a virtual tid to its flat index.
    /// @param tid Virtual title id.
    /// @return Flat index, or -1 when not a live virtual title.
    int IndexOf(u64 tid) {
        int slot = -1;
        int index = -1;
        if (!DecodeTid(tid, slot, index))
            return -1;

        for (int vi = 0; vi < s_count; vi++) {
            if (s_entries[vi].provider == slot && s_entries[vi].index == index)
                return vi;
        }

        return -1;
    }

    /// @brief Test whether a tid addresses a virtual title.
    /// @param tid Title id to test.
    /// @return True for virtual tids.
    bool IsId(u64 tid) {
        int slot = -1;
        int index = -1;
        return DecodeTid(tid, slot, index);
    }

    /// @brief Give the donor backing a virtual title, if any.
    /// @param tid Virtual title id.
    /// @return Donor title id, or 0 for plain virtual titles.
    u64 DonorOf(u64 tid) {
        int slot = -1;
        int index = -1;
        if (!DecodeTid(tid, slot, index))
            return 0;

        if (!s_providers[slot].donor)
            return 0;

        u64 donor = s_providers[slot].donor(index);
        /* Donors are real titles; never accept virtual tids back. */
        if (donor == 0 || (donor & kVirtualBit) != 0)
            return 0;
        return donor;
    }

    /// @brief Give the NRO booted under a virtual title's donor.
    /// @param tid Virtual title id.
    /// @param out Output buffer for the sdmc:/... path.
    /// @param cap Buffer capacity.
    /// @return Path length (<0 when the title is not NRO-backed).
    int NroOf(u64 tid, char *out, unsigned cap) {
        int slot = -1;
        int index = -1;
        if (!DecodeTid(tid, slot, index) || !out || !cap)
            return -1;

        if (!s_providers[slot].nro)
            return -1;

        out[0] = '\0';
        int len = s_providers[slot].nro(index, out, cap);
        if (len < 0 || out[0] == '\0')
            return -1;

        out[cap - 1] = '\0';
        return (int)strlen(out);
    }

    /// @brief Dispatch a launch to the owning provider.
    /// NRO-backed titles never reach here (the menu routes them to
    /// nro::Launch); plain titles run their TitleLaunch hook.
    /// @param tid Virtual title id.
    /// @return 0 when handled, libnx error otherwise.
    Result Launch(u64 tid) {
        int slot = -1;
        int index = -1;
        if (!DecodeTid(tid, slot, index)) {
            logging::LogLine("[vtitles] Bad launch (tid: %016llX)", (unsigned long long)tid);
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }

        if (!s_providers[slot].launch) {
            logging::LogLine("[vtitles] NRO title cannot use provider launch (tid: %016llX)",
                (unsigned long long)tid);
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }

        logging::LogLine("[vtitles] Launching provider item (slot: %d index: %d)", slot, index);
        int rc = s_providers[slot].launch(index);
        if (rc != 0) {
            logging::LogLine("[vtitles] Provider refused launch (rc: %d)", rc);
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }

        return 0;
    }

    /// @brief Consume a pending virtual-set change notification.
    /// @return True once when providers or counts changed.
    bool Changed() {
        bool changed = s_changed;
        s_changed = false;
        return changed;
    }

} // namespace vtitles
