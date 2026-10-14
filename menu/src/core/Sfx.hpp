/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once

/// @brief Sound effect registry and mixer.
///
/// Clips are data driven: `menu/src/assets/sfx.json` (embedded) declares
/// `{id, asset, db, jitter}` rows, and `sdmc:/qlaunch-ext/sfx.json` may
/// override them. `asset` names resolve to embedded WAV blobs; `db` sets
/// per-clip gain; `jitter` enables the hover pitch shimmer. Callers play
/// clips by string id so menu jingles stay out of code.
namespace Sfx {

    /// @brief Legacy clip ids. Kept for compatibility; each maps to a
    /// data-driven clip name (see kLegacyNames in Sfx.cpp).
    enum Id { Hover, Click, Back, AlbumJ, SettingsJ, HomeJ, HomeBoot, EshopJ, ChatJ, FolderJ, VgcJ, StandbyJ, Count };

    /// @brief Initialize audio output and load the clip registry.
    /// @return True when audio is ready (registry still loads on failure).
    bool Init();

    /// @brief Shut down voices and audio output.
    void Shutdown();

    /// @brief Pump one mixer tick into the audio queue. Call every frame.
    void Tick();

    /// @brief Play a legacy clip id.
    /// @param id Legacy clip id.
    /// @param pitch Playback rate multiplier, clamped 0.25-4.0.
    void Play(Id id, float pitch = 1.0f);

    /// @brief Play a clip by its data-driven id.
    /// @param name Clip id from sfx.json (for example "home_jingle").
    /// @param pitch Playback rate multiplier, clamped 0.25-4.0.
    void PlayByName(const char *name, float pitch = 1.0f);

    /// @brief Resolve a clip name to its registry slot.
    /// @param name Clip id from sfx.json.
    /// @return Slot index, or -1 when missing.
    int IdOf(const char *name);

    /// @brief Number of clips in the registry.
    /// @return Clip count.
    int ClipCount();

    /// @brief Look up a clip id by slot.
    /// @param slot Registry slot from IdOf().
    /// @return Clip id string, or "" when out of range.
    const char *ClipName(int slot);

    /// @brief Load the clip registry from a JSON buffer.
    /// @param json JSON text with a "clips" array.
    /// @param len Text length in bytes.
    /// @return True when at least one clip loaded.
    bool LoadJson(const char *json, unsigned len);

    /// @brief Load the embedded sfx.json registry.
    /// @return True on success.
    bool LoadBuiltin();

    /// @brief Load a registry override from an SD file.
    /// @param path Filesystem path to sfx.json.
    /// @return True on success.
    bool LoadFile(const char *path);

    /// @brief Load the SD override, else the embedded registry.
    /// @return True on success.
    bool LoadAuto();

    /// @brief Mute UI voices and start queuing video PCM.
    void VideoBegin();

    /// @brief Stop video PCM mode and drain the video buffer.
    void VideoEnd();

    /// @brief Push interleaved stereo PCM into the video buffer.
    /// @param pcm Interleaved frames, or null to ignore.
    /// @param frames Frame count (stereo pairs).
    void VideoPush(const short *pcm, unsigned frames);

    /// @brief Seek the video clock without pushing samples.
    /// @param secs New clock position in seconds.
    void VideoSeekTo(double secs);

    /// @brief Whether the video buffer wants more PCM.
    /// @return True when below the refill threshold.
    bool VideoHungry();

    /// @brief Give the video clock in seconds.
    /// @return Seconds played.
    double VideoTime();

} /* namespace Sfx */
