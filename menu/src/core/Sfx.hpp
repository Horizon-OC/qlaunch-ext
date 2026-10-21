/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once

/// @brief Sound effect registry and mixer.
///
/// Clips are data driven: `menu/src/assets/sfx.json` (embedded) declares
/// `{id, asset, db, jitter}` rows, and `<pack>/assets/sfx.json` may
/// override them. `asset` names resolve to embedded WAV blobs, or to
/// `<pack>/assets/sfx/<asset>.wav` when the pack registry is active;
/// `db` sets per-clip gain; `jitter` enables the hover pitch shimmer.
/// Callers play clips by string id so menu jingles stay out of code.
namespace Sfx {

    /// @brief Initialize SDL audio output and load the pack clip registry.
    /// Clips come only from the pack (assets/sfx.json + assets/sfx/*.wav).
    /// @param packDir Theme pack directory (may be null).
    /// @return True when audio is ready (registry may still be empty).
    bool Init(const char *packDir);

    /// @brief Shut down voices and the SDL audio stream.
    void Shutdown();

    /// @brief Mix one tick and queue it on the SDL audio stream. Call every frame.
    void Tick();

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

    /// @brief Load the pack assets/sfx.json registry (pack-only, no fallback).
    /// @param packDir Theme pack directory (may be null).
    /// @return True when at least one clip loaded.
    bool LoadAuto(const char *packDir);

    /// @brief Mute UI voices and start queuing video PCM.
    /// @note The ring is written by the video worker thread and drained by
    /// Tick() on the main thread; all entry points are thread-safe.
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

    /// @brief Give buffered video audio in seconds.
    /// @return Buffered seconds (0 when empty).
    double VideoBuffered();

    /// @brief Hold or release video PCM output (pause without draining).
    /// @param paused True to output silence and keep buffered PCM.
    void VideoSetPaused(bool paused);

    /// @brief Give the video clock in seconds.
    /// @return Seconds played.
    double VideoTime();

} /* namespace Sfx */
