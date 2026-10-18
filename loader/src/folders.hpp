/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */

#pragma once
#include <switch.h>
#include <stdio.h>

namespace folders {

constexpr int kFolderMax = 8;
constexpr int kFolderMaxGames = 24;
constexpr int kFolderColorCount = 10;
constexpr int kFolderNameBytes = 96;

/// @brief Folder count.
/// @return Folder count.
int Count();

/// @brief Give a folder id by index.
/// @param index Folder index.
/// @return Folder id, or 0 when out of range.
u64 Id(int index);

/// @brief Read a folder name into a buffer.
/// @param index Folder index.
/// @param out Output buffer.
/// @param cap Buffer capacity.
/// @return Bytes written, or negative on failure.
int Name(int index, char *out, unsigned cap);

/// @brief Give a folder's game count.
/// @param index Folder index.
/// @return Game count, or negative on failure.
int GameCount(int index);

/// @brief Give a member title id.
/// @param index Folder index.
/// @param game Member position.
/// @return Title id, or 0 when out of range.
u64 Game(int index, int game);

/// @brief Find a folder by id.
/// @param id Folder id.
/// @return Index, or negative when missing.
int IndexOfId(u64 id);

/// @brief Find the folder holding a game.
/// @param tid Game title id.
/// @return Index, or negative when in no folder.
/// @brief Give a folder's palette index.
/// @param index Folder index.
/// @return Palette index, or negative on failure.
int Color(int index);

/// @brief Find the folder holding a game.
/// @param tid Game title id.
/// @return Folder index, or negative when in no folder.
int IndexOfGame(u64 tid);

/// @brief Create a folder holding one game.
/// Folders always start non-empty; there is no empty-folder state.
/// @param name Folder name (empty names rejected).
/// @param tid First game.
/// @return Folder id, or 0 on failure.
u64 Create(const char *name, u64 tid);

/// @brief Rename a folder.
/// @param id Folder id.
/// @param name New name (empty names rejected).
/// @return 0 on success, negative on failure.
int Rename(u64 id, const char *name);

/// @brief Set a folder's palette index.
/// @param id Folder id.
/// @param color Palette index (clamped to the palette).
/// @return 0 on success, negative on failure.
int SetColor(u64 id, int color);

/// @brief Delete a folder, returning its games to the strip.
/// @param id Folder id.
/// @return 0 on success, negative on failure.
int Delete(u64 id);

/// @brief Add a game to a folder.
/// @param id Folder id.
/// @param tid Game title id.
/// @return 0 on success, negative on failure.
int Add(u64 id, u64 tid);

/// @brief Remove a game from a folder.
/// Emptied folders are deleted automatically.
/// @param id Folder id.
/// @param tid Game title id.
/// @return 0 on success, negative on failure.
int Remove(u64 id, u64 tid);

/// @brief Drop member tids missing from the live title list.
/// Emptied folders are deleted automatically.
/// @param liveTids Live title ids.
/// @param liveCount Live title count.
void Prune(const u64 *liveTids, int liveCount);

/// @brief Write the folders section («"folders": {...}») for order.json.
/// @param f Output file.
void WriteSection(FILE *f);

} // namespace folders
