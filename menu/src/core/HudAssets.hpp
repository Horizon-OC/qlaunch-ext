/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include "Icons.hpp"

/* Compiled-in PNG blobs (bin2s objects from menu/assets/hud). */
extern const uint8_t hud_user_png[];
extern const uint32_t hud_user_png_size;
extern const uint8_t hud_btn_a_png[];
extern const uint32_t hud_btn_a_png_size;
extern const uint8_t hud_btn_plus_png[];
extern const uint32_t hud_btn_plus_png_size;
extern const uint8_t hud_folder_png[];
extern const uint32_t hud_folder_png_size;
extern const uint8_t hud_vgccard_png[];
extern const uint32_t hud_vgccard_png_size;
extern const uint8_t top_sel_home_png[];
extern const uint32_t top_sel_home_png_size;
extern const uint8_t top_sel_connect_png[];
extern const uint32_t top_sel_connect_png_size;
extern const uint8_t top_sel_eshop_png[];
extern const uint32_t top_sel_eshop_png_size;
extern const uint8_t top_sel_album_png[];
extern const uint32_t top_sel_album_png_size;
extern const uint8_t top_sel_settings_png[];
extern const uint32_t top_sel_settings_png_size;
extern const uint8_t top_des_home_png[];
extern const uint32_t top_des_home_png_size;
extern const uint8_t top_des_connect_png[];
extern const uint32_t top_des_connect_png_size;
extern const uint8_t top_des_eshop_png[];
extern const uint32_t top_des_eshop_png_size;
extern const uint8_t top_des_album_png[];
extern const uint32_t top_des_album_png_size;
extern const uint8_t top_des_settings_png[];
extern const uint32_t top_des_settings_png_size;
extern const uint8_t marker_sheet_png[];
extern const uint32_t marker_sheet_png_size;
extern const uint8_t wifi_0_png[];
extern const uint32_t wifi_0_png_size;
extern const uint8_t wifi_1_png[];
extern const uint32_t wifi_1_png_size;
extern const uint8_t wifi_2_png[];
extern const uint32_t wifi_2_png_size;
extern const uint8_t wifi_3_png[];
extern const uint32_t wifi_3_png_size;
extern const uint8_t wifi_none_png[];
extern const uint32_t wifi_none_png_size;
extern const uint8_t airplane_png[];
extern const uint32_t airplane_png_size;

namespace HudAssets {

    /// @brief Compiled-in HUD table. Row order must match the HUD_* ids;
    /// adding an icon is a new id plus one row here, no engine changes.
    inline const Icons::HudDef kDefs[] = {
        { hud_user_png, &hud_user_png_size, false },                 /* kHudUser */
        { hud_btn_a_png, &hud_btn_a_png_size, false },               /* kHudButtonA */
        { hud_btn_plus_png, &hud_btn_plus_png_size, false },         /* kHudButtonPlus */
        { hud_folder_png, &hud_folder_png_size, false },             /* kHudFolder */
        { hud_vgccard_png, &hud_vgccard_png_size, false },           /* kHudVgcCard */
        { top_sel_home_png, &top_sel_home_png_size, false },         /* kHudTopSelHome */
        { top_sel_connect_png, &top_sel_connect_png_size, false },   /* kHudTopSelConnect */
        { top_sel_eshop_png, &top_sel_eshop_png_size, false },       /* kHudTopSelEshop */
        { top_sel_album_png, &top_sel_album_png_size, false },       /* kHudTopSelAlbum */
        { top_sel_settings_png, &top_sel_settings_png_size, false }, /* kHudTopSelSettings */
        { top_des_home_png, &top_des_home_png_size, false },         /* kHudTopDesHome */
        { top_des_connect_png, &top_des_connect_png_size, false },   /* kHudTopDesConnect */
        { top_des_eshop_png, &top_des_eshop_png_size, false },       /* kHudTopDesEshop */
        { top_des_album_png, &top_des_album_png_size, false },       /* kHudTopDesAlbum */
        { top_des_settings_png, &top_des_settings_png_size, false }, /* kHudTopDesSettings */
        { marker_sheet_png, &marker_sheet_png_size, false },         /* kHudMarkerStrip */
        { wifi_0_png, &wifi_0_png_size, false },                     /* kHudWifi0 */
        { wifi_1_png, &wifi_1_png_size, false },                     /* kHudWifi1 */
        { wifi_2_png, &wifi_2_png_size, false },                     /* kHudWifi2 */
        { wifi_3_png, &wifi_3_png_size, false },                     /* kHudWifi3 */
        { wifi_none_png, &wifi_none_png_size, false },               /* kHudWifiNone */
        { airplane_png, &airplane_png_size, false },                 /* kHudAirplane */
    };

    /// @brief Number of rows in the HUD table.
    constexpr int kCount = (int)(sizeof(kDefs) / sizeof(kDefs[0]));

    static_assert(kCount == kHudCount, "HudAssets table must cover every HUD_* id");

} /* namespace HudAssets */
