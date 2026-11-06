/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */
#pragma once
#include <switch.h>
#include <deko3d.h>
/* Bounded LRU title icon cache (sLaunch-style: fixed capacity, texture reuse,
   per-pass decode budget). Replaces the old fixed 6-entry array. */
constexpr int kIconCacheCap = 48;
constexpr int kIconScaleMax = 192;
constexpr int kHudUser = 0;
constexpr int kHudButtonA = 1;
constexpr int kHudButtonPlus = 2;
constexpr int kHudFolder = 3;
constexpr int kHudTopSelHome = 4;
constexpr int kHudTopSelConnect = 5;
constexpr int kHudTopSelEshop = 6;
constexpr int kHudTopSelAlbum = 7;
constexpr int kHudTopSelSettings = 8;
constexpr int kHudTopDesHome = 9;
constexpr int kHudTopDesConnect = 10;
constexpr int kHudTopDesEshop = 11;
constexpr int kHudTopDesAlbum = 12;
constexpr int kHudTopDesSettings = 13;
constexpr int kHudMarkerStrip = 14;
constexpr int kHudWifi0 = 15;
constexpr int kHudWifi1 = 16;
constexpr int kHudWifi2 = 17;
constexpr int kHudWifi3 = 18;
constexpr int kHudWifiNone = 19;
constexpr int kHudAirplane = 20;
constexpr int kHudCount = 21;
struct IconReq {
    u64 titleId;
    int rowIndex;
};
namespace Icons {
    void Sync(const u64 *titleIds, int *outputSlots, int rowCount, int focusedRow, int maxLoadsPerPass = 2);
    void FreeAll();
    void ResetHud();
    bool InitHudPack(const char *packDir);
    bool ReapplyPack();
    int HudSlot(int hudId);
    struct Entry {
        u64 titleId;
        bool isUsed;
        u64 sequence;
        int textureSlot;
    };
    bool Upload(Entry *iconEntry, const uint8_t *pixelData, int imageWidth, int imageHeight);
    bool UploadPng(const uint8_t *pngData, unsigned pngSize, bool downscale, int *textureSlotOut);
    void Free(Entry *iconEntry);
    void PurgeHome();
    void TrimTitleIcons();
    bool TrimTitleIconsStep();
} /* namespace Icons */
