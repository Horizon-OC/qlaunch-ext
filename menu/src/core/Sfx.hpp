/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once

namespace Sfx {

enum Id { Hover, Click, Back, AlbumJ, SettingsJ, HomeJ, HomeBoot, EshopJ, ChatJ, FolderJ, VgcJ, StandbyJ, Count };

bool Init();
void Shutdown();
void Tick();
void Play(Id id, float pitch = 1.0f);

} /* namespace Sfx */
