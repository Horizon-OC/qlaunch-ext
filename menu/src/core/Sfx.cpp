/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Sfx.hpp"
#include "Assets.hpp"
#include "Json.hpp"
#include "Qext.hpp"
#include "Packs.hpp"
#include <SDL3/SDL.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

namespace Sfx {

    namespace {

        /// @brief Maximum data-driven clips in the registry.
        constexpr int kMaxClips = 24;

        /// @brief Maximum length of a clip id string.
        constexpr int kIdMax = 32;

        /// @brief Output sample rate in Hz.
        constexpr int kOutRate = 48000;

        /// @brief Output channel count (stereo).
        constexpr int kOutCh = 2;

        /// @brief Mixer frames rendered per Tick().
        constexpr int kFramesPerTick = 800;

        /// @brief Ticks of PCM allowed to sit in the SDL audio queue.
        constexpr int kQueueTicks = 4;

        /// @brief Simultaneous UI voice count.
        constexpr int kVoices = 8;

        /// @brief Decoded PCM clip plus playback gain.
        struct Clip {
            const short *pcm;
            unsigned frames;
            unsigned ch;
            unsigned rate;
            float gain;
        };

        /// @brief Live voice playing one clip.
        struct Voice {
            const short *pcm;
            unsigned frames;
            unsigned ch;
            unsigned step;
            unsigned pos;
            float vol;
            bool on;
        };

        /// @brief Registry entry: data-driven id plus decoded clip.
        struct RegClip {
            char id[kIdMax];
            Clip clip;
            bool jitter;
        };

        /// @brief Clip registry backing store.
        RegClip s_registry[kMaxClips];

        /// @brief Number of live entries in s_registry.
        int s_registryCount = 0;

        /// @brief Live mixer voices.
        Voice s_voices[kVoices];

        /// @brief Round-robin cursor into s_voices.
        unsigned s_voiceCursor = 0;

        /// @brief True once audio output is open.
        bool s_isReady = false;

        /// @brief Device-bound SDL audio stream (null when closed).
        SDL_AudioStream *s_audioStream = 0;

        /// @brief Scratch buffer holding one mixed tick.
        short s_mixBuffer[kFramesPerTick * kOutCh];

        /// @brief Ticks elapsed (seeds hover jitter).
        unsigned s_tickCount = 0;

        /// @brief Successful stream pushes.
        unsigned s_appendCount = 0;

        /// @brief Dropped or rejected stream pushes.
        unsigned s_appendFailCount = 0;

        /// @brief Total Play() calls accepted.
        unsigned s_playCount = 0;

        /// @brief RNG state for hover pitch jitter.
        unsigned s_randomState = 0x12345678;

        /// @brief True while video PCM mode mutes UI voices.
        bool s_videoMode = false;

        /// @brief True while paused: Tick() outputs silence, keeps the ring.
        bool s_videoPaused = false;

        /// @brief Guards the video ring (worker pushes, main thread drains).
        pthread_mutex_t s_videoMutex = PTHREAD_MUTEX_INITIALIZER;

        /// @brief Video PCM ring capacity in shorts (2s of stereo @48kHz).
        /// Mirrors sLaunch's two-second audio ring so the demuxer can read
        /// ~0.6s ahead without the mixer running dry (choppy audio).
        static constexpr unsigned kVidCap = 48000u * 2u * 2u;

        /// @brief Refill threshold in shorts (0.6s of stereo @48kHz).
        static constexpr unsigned kVidHungry = 48000u * 2u * 6u / 10u;

        /// @brief Video PCM ring buffer (interleaved stereo shorts).
        short s_videoBuffer[48000u * 2u * 2u];

        /// @brief Read head into s_videoBuffer.
        unsigned s_videoHead = 0;

        /// @brief Buffered (unplayed) sample count.
        unsigned s_videoLength = 0;

        /// @brief Total video samples queued since begin/seek.
        unsigned s_videoPlayed = 0;

        /// @brief Read a little-endian u32 from raw bytes.
        /// @param p Byte pointer (4 bytes available).
        /// @return Decoded value.
        unsigned Rd32(const unsigned char *p)
        {
            return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
        }

        /// @brief Parse a 16-bit PCM WAV blob into a Clip.
        /// @param d WAV bytes.
        /// @param n Byte count.
        /// @param c Output clip (zeroed on failure).
        /// @return True when the blob is 16-bit PCM mono/stereo.
        bool ParseWav(const unsigned char *d, unsigned n, Clip &c)
        {
            c.pcm = 0;
            c.frames = 0;
            c.ch = 0;
            c.rate = 0;

            if (!d || n < 44) {
                return false;
            }

            if (memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0) {
                return false;
            }

            unsigned fmt = 0;
            unsigned bits = 0;
            unsigned ch = 0;
            unsigned rate = 0;
            const short *data = 0;
            unsigned dframes = 0;
            unsigned off = 12;

            while (off + 8 <= n) {
                unsigned id = Rd32(d + off);
                unsigned sz = Rd32(d + off + 4);
                unsigned body = off + 8;

                if (body > n || sz > n) {
                    break;
                }

                unsigned avail = n - body;
                unsigned take = sz < avail ? sz : avail;

                if (id == 0x20746D66u) {
                    if (take >= 16) {
                        fmt = (unsigned)d[body] | ((unsigned)d[body + 1] << 8);
                        ch = (unsigned)d[body + 2] | ((unsigned)d[body + 3] << 8);
                        rate = Rd32(d + body + 4);
                        bits = (unsigned)d[body + 14] | ((unsigned)d[body + 15] << 8);
                    }
                } else if (id == 0x61746164u) {
                    if (ch >= 1 && ch <= 2 && take >= 2) {
                        data = (const short *)(d + body);
                        dframes = take / (2u * ch);
                    }
                }

                off = body + sz + (sz & 1u);
            }

            if (fmt != 1 || bits != 16 || ch < 1 || ch > 2 || !data || !dframes) {
                return false;
            }

            c.pcm = data;
            c.frames = dframes;
            c.ch = ch;
            c.rate = rate;
            return true;
        }

        /// @brief Clear the clip registry, freeing owned PCM.
        void ClearReg()
        {
            for (int i = 0; i < s_registryCount; i++) {
                free((void *)s_registry[i].clip.pcm);
                s_registry[i].clip.pcm = nullptr;
            }

            memset(s_registry, 0, sizeof(s_registry));
            s_registryCount = 0;
        }

        /// @brief Store one decoded clip, replacing any same-id entry.
        /// @param id Clip id.
        /// @param clip Decoded clip with gain applied.
        /// @param jitter True to enable hover-style pitch shimmer.
        /// @return Slot index, or -1 when the registry is full.
        int StoreClip(const char *id, const Clip &clip, bool jitter)
        {
            if (!id || !id[0] || !clip.pcm || !clip.frames || !clip.rate) {
                return -1;
            }

            for (int i = 0; i < s_registryCount; i++) {
                if (strncmp(s_registry[i].id, id, kIdMax) == 0) {
                    free((void *)s_registry[i].clip.pcm);
                    s_registry[i].clip = clip;
                    s_registry[i].jitter = jitter;
                    return i;
                }
            }

            if (s_registryCount >= kMaxClips) {
                return -1;
            }

            RegClip *r = &s_registry[s_registryCount];
            memset(r, 0, sizeof(*r));
            size_t len = strlen(id);

            if (len > (size_t)(kIdMax - 1)) {
                len = (size_t)(kIdMax - 1);
            }

            memcpy(r->id, id, len);
            r->id[len] = 0;
            r->clip = clip;
            r->jitter = jitter;
            return s_registryCount++;
        }

        /// @brief Decode one WAV blob and store it under a clip id.
        /// Takes ownership of a copy of the decoded PCM, so the caller's
        /// WAV buffer may be freed right after this returns.
        /// @param id Clip id.
        /// @param wavData WAV bytes.
        /// @param wavSize Byte count.
        /// @param db Gain in decibels.
        /// @param jitter True to enable pitch shimmer.
        void StoreDecoded(const char *id, const unsigned char *wavData, unsigned wavSize,
                          float db, bool jitter)
        {
            Clip c;
            c.gain = 1.0f;

            if (ParseWav(wavData, wavSize, c) && c.rate >= 8000 && c.rate <= 96000) {
                unsigned pcmBytes = c.frames * (unsigned)c.ch * sizeof(short);
                short *ownedPcm = (short *)malloc(pcmBytes);

                if (!ownedPcm) {
                    logging::LogLine("[sfx] out of memory for clip '%s'", id ? id : "?");
                    return;
                }

                memcpy(ownedPcm, c.pcm, pcmBytes);
                c.pcm = ownedPcm;
                c.gain = powf(10.0f, db / 20.0f);

                if (StoreClip(id, c, jitter) >= 0) {
                    logging::LogLine("[sfx] clip '%s': %u frames %u ch %u Hz", id, c.frames, c.ch, c.rate);
                } else {
                    free(ownedPcm);
                }
            } else {
                logging::LogLine("[sfx] bad clip '%s'", id ? id : "?");
            }
        }

        /// @brief Whether an sfx asset name is safe to join into a pack path.
        /// @param assetName Asset name from sfx.json.
        /// @return True when separator/traversal free.
        bool IsAssetNameUsable(const char *assetName)
        {
            if (!assetName || !assetName[0] || strlen(assetName) > 31) {
                return false;
            }

            for (const char *cursor = assetName; *cursor; cursor++) {
                char current = *cursor;
                bool lower = current >= 'a' && current <= 'z';
                bool digit = current >= '0' && current <= '9';

                if (!lower && !digit && current != '_') {
                    return false;
                }
            }

            return true;
        }

        /// @brief Load one clip from its pack wav file (pack-only, no fallback).
        /// Missing or unreadable wavs log and leave the clip unregistered.
        /// @param id Clip id.
        /// @param asset Asset name (pack assets/sfx/<asset>.wav).
        /// @param db Gain in decibels.
        /// @param jitter True to enable pitch shimmer.
        /// @param packDir Theme pack directory (may be null).
        void LoadOnePack(const char *id, const char *asset, float db, bool jitter,
                         const char *packDir)
        {
            if (!packDir || !packDir[0] || !IsAssetNameUsable(asset)) {
                logging::LogLine("[sfx] bad asset '%s' for clip '%s'", asset ? asset : "?",
                                 id ? id : "?");
                return;
            }

            char wavPath[256];
            snprintf(wavPath, sizeof(wavPath), "%s/assets/sfx/%s.wav", packDir, asset);
            wavPath[sizeof(wavPath) - 1] = 0;

            unsigned wavSize = 0;
            unsigned char *wavData = Packs::ReadFile(wavPath, 4 * 1024 * 1024, &wavSize);

            if (!wavData) {
                logging::LogLine("[sfx] missing '%s' for clip '%s'", wavPath, id ? id : "?");
                return;
            }

            StoreDecoded(id, wavData, wavSize, db, jitter);
            free(wavData);
        }

        /// @brief Read a float field from a JSON object token.
        /// @param d Parsed document.
        /// @param obj Object token.
        /// @param key Field name.
        /// @param dflt Default when missing or malformed.
        /// @return Parsed value or default.
        float JFloat(const JxDoc &d, int obj, const char *key, float dflt)
        {
            int t = Jx::ObjGet(d, obj, key);

            if (Jx::TokType(d, t) != 4) {
                return dflt;
            }

            int len = d.toks[t].end - d.toks[t].start;

            if (len <= 0 || len > 32) {
                return dflt;
            }

            char buf[40];
            memcpy(buf, d.text + d.toks[t].start, (unsigned)len);
            buf[len] = 0;
            return strtof(buf, NULL);
        }

        /// @brief Read a boolean field from a JSON object token.
        /// @param d Parsed document.
        /// @param obj Object token.
        /// @param key Field name.
        /// @param dflt Default when missing.
        /// @return Parsed value or default.
        bool JFlag(const JxDoc &d, int obj, const char *key, bool dflt)
        {
            int t = Jx::ObjGet(d, obj, key);

            if (Jx::TokType(d, t) != 4) {
                return dflt;
            }

            return Jx::IsTrue(d, t);
        }

        /// @brief Read a string field from a JSON object token.
        /// @param d Parsed document.
        /// @param obj Object token.
        /// @param key Field name.
        /// @param out Output buffer.
        /// @param cap Buffer capacity.
        void JName(const JxDoc &d, int obj, const char *key, char *out, unsigned cap)
        {
            int t = Jx::ObjGet(d, obj, key);

            if (Jx::Str(d, t, out, cap) < 0 && cap) {
                out[0] = 0;
            }
        }

        /// @brief Start a voice for a registry slot.
        /// @param slot Registry slot from IdOf().
        /// @param pitch Playback rate multiplier, clamped 0.25-4.0.
        void StartVoice(int slot, float pitch)
        {
            if (!s_isReady || slot < 0 || slot >= s_registryCount) {
                return;
            }

            if (s_videoMode) {
                return;
            }

            const RegClip &r = s_registry[slot];

            if (!r.clip.pcm || !r.clip.frames || !r.clip.rate) {
                return;
            }

            Voice &v = s_voices[s_voiceCursor];
            s_voiceCursor = (s_voiceCursor + 1u) % kVoices;
            v.pcm = r.clip.pcm;
            v.frames = r.clip.frames;
            v.ch = r.clip.ch;

            float p = pitch;

            if (r.jitter) {
                s_randomState = s_randomState * 1664525u + 1013904223u + (s_tickCount++);
                float u = (float)(s_randomState >> 8) * (1.0f / 16777216.0f);
                p = powf(1.3f, u * 2.0f - 1.0f);
            }

            if (p < 0.25f) {
                p = 0.25f;
            } else if (p > 4.0f) {
                p = 4.0f;
            }

            v.step = (unsigned)((double)r.clip.rate * (double)p * 65536.0 / (double)kOutRate);

            if (!v.step) {
                v.step = 1;
            }

            v.pos = 0;
            v.vol = r.clip.gain;
            v.on = true;
            s_playCount++;
        }

    } // namespace

    /// @brief Initialize SDL audio output and load the pack clip registry.
    /// Clips come only from the pack (assets/sfx.json + assets/sfx/*.wav);
    /// a missing registry logs and leaves playback silent.
    /// @param packDir Theme pack directory (may be null).
    /// @return True when audio is ready (registry may still be empty).
    bool Init(const char *packDir)
    {
        if (s_isReady) {
            return true;
        }

        memset(s_voices, 0, sizeof(s_voices));
        memset(s_mixBuffer, 0, sizeof(s_mixBuffer));

        if (!LoadAuto(packDir)) {
            logging::LogLine("[sfx] no pack registry, playback silent");
        }

        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            logging::LogLine("[sfx] SDL audio init failed: %s", SDL_GetError());
            return false;
        }

        SDL_AudioSpec spec;
        spec.format = SDL_AUDIO_S16;
        spec.channels = kOutCh;
        spec.freq = kOutRate;
        s_audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);

        if (!s_audioStream) {
            logging::LogLine("[sfx] Can't open stream, error: %s", SDL_GetError());
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return false;
        }

        if (!SDL_ResumeAudioStreamDevice(s_audioStream)) {
            logging::LogLine("[sfx] Can't resume stream, error: %s", SDL_GetError());
            SDL_DestroyAudioStream(s_audioStream);
            s_audioStream = 0;
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return false;
        }

        s_isReady = true;
        return true;
    }

    /// @brief Shut down voices and audio output.
    void Shutdown()
    {
        for (unsigned i = 0; i < kVoices; i++) {
            s_voices[i].on = false;
        }

        if (s_audioStream) {
            SDL_DestroyAudioStream(s_audioStream);
            s_audioStream = 0;
        }

        if (s_isReady) {
            s_isReady = false;
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
    }

    /// @brief Play a clip by its data-driven id.
    /// @param name Clip id from sfx.json (for example "home_jingle").
    /// @param pitch Playback rate multiplier, clamped 0.25-4.0.
    void PlayByName(const char *name, float pitch)
    {
        StartVoice(IdOf(name), pitch);
    }

    /// @brief Resolve a clip name to its registry slot.
    /// @param name Clip id from sfx.json.
    /// @return Slot index, or -1 when missing.
    int IdOf(const char *name)
    {
        if (!name || !name[0]) {
            return -1;
        }

        for (int i = 0; i < s_registryCount; i++) {
            if (strncmp(s_registry[i].id, name, kIdMax) == 0) {
                return i;
            }
        }

        return -1;
    }

    /// @brief Number of clips in the registry.
    /// @return Clip count.
    int ClipCount()
    {
        return s_registryCount;
    }

    /// @brief Look up a clip id by slot.
    /// @param slot Registry slot from IdOf().
    /// @return Clip id string, or "" when out of range.
    const char *ClipName(int slot)
    {
        if (slot < 0 || slot >= s_registryCount) {
            return "";
        }

        return s_registry[slot].id;
    }

    /// @brief Load the clip registry from a JSON buffer with pack wav lookup.
    /// @param json JSON text with a "clips" array.
    /// @param len Text length in bytes.
    /// @param packDir Theme pack directory for assets/sfx/*.wav (may be null).
    /// @return True when at least one clip loaded.
    bool LoadJsonPack(const char *json, unsigned len, const char *packDir)
    {
        JxDoc doc;

        if (!Jx::Parse(doc, json, len)) {
            return false;
        }

        if (Jx::TokType(doc, 0) != 1) {
            return false;
        }

        int clips = Jx::ObjGet(doc, 0, "clips");

        if (Jx::TokType(doc, clips) != 2) {
            return false;
        }

        ClearReg();
        int n = Jx::ArrSize(doc, clips);

        for (int i = 0; i < n; i++) {
            int on = Jx::ArrAt(doc, clips, i);

            if (Jx::TokType(doc, on) != 1) {
                continue;
            }

            char id[kIdMax] = { 0 };
            char asset[32] = { 0 };
            JName(doc, on, "id", id, sizeof(id));
            JName(doc, on, "asset", asset, sizeof(asset));

            if (!id[0] || !asset[0]) {
                continue;
            }

            float db = JFloat(doc, on, "db", 0.0f);
            bool jitter = JFlag(doc, on, "jitter", false);
            LoadOnePack(id, asset, db, jitter, packDir);
        }

        return s_registryCount > 0;
    }

    /// @brief Load the pack assets/sfx.json registry (pack-only, no fallback).
    /// @param packDir Theme pack directory (may be null).
    /// @return True when at least one clip loaded.
    bool LoadAuto(const char *packDir)
    {
        if (!packDir || !packDir[0]) {
            return false;
        }

        char jsonPath[256];
        snprintf(jsonPath, sizeof(jsonPath), "%s/assets/sfx.json", packDir);
        jsonPath[sizeof(jsonPath) - 1] = 0;

        unsigned jsonSize = 0;
        unsigned char *jsonData = Packs::ReadFile(jsonPath, 65536, &jsonSize);

        if (!jsonData) {
            return false;
        }

        bool jsonOk = LoadJsonPack((const char *)jsonData, jsonSize, packDir);
        free(jsonData);
        return jsonOk;
    }

    /// @brief Mix one tick and queue it on the SDL audio stream. Call every frame.
    void Tick()
    {
        if (!s_isReady || !s_audioStream) {
            return;
        }

        short *dst = s_mixBuffer;

        if (s_videoMode) {
            pthread_mutex_lock(&s_videoMutex);

            if (s_videoPaused) {
                for (unsigned f = 0; f < (unsigned)kFramesPerTick * (unsigned)kOutCh; f++) {
                    dst[f] = 0;
                }

                pthread_mutex_unlock(&s_videoMutex);
            } else {
                for (unsigned f = 0; f < (unsigned)kFramesPerTick * (unsigned)kOutCh; f++) {
                    if (s_videoLength > 0) {
                        dst[f] = s_videoBuffer[s_videoHead];
                        s_videoHead = (s_videoHead + 1u) % kVidCap;
                        s_videoLength--;
                    } else {
                        dst[f] = 0;
                    }
                }

                pthread_mutex_unlock(&s_videoMutex);
            }

            s_videoPlayed += kFramesPerTick;
        } else {
            for (unsigned f = 0; f < (unsigned)kFramesPerTick; f++) {
                int l = 0;
                int r = 0;

                for (unsigned vi = 0; vi < kVoices; vi++) {
                    Voice &v = s_voices[vi];

                    if (!v.on) {
                        continue;
                    }

                    unsigned idx = v.pos >> 16;

                    if (idx >= v.frames) {
                        v.on = false;
                        continue;
                    }

                    unsigned fr8 = (v.pos >> 8) & 255u;
                    unsigned nx = idx + 1u < v.frames ? idx + 1u : idx;
                    int s0l;
                    int s0r;
                    int s1l;
                    int s1r;

                    if (v.ch == 1) {
                        s0l = v.pcm[idx];
                        s0r = s0l;
                        s1l = v.pcm[nx];
                        s1r = s1l;
                    } else {
                        s0l = v.pcm[idx * 2u];
                        s0r = v.pcm[idx * 2u + 1u];
                        s1l = v.pcm[nx * 2u];
                        s1r = v.pcm[nx * 2u + 1u];
                    }

                    l += (int)((float)(s0l + (((s1l - s0l) * (int)fr8) >> 8)) * v.vol);
                    r += (int)((float)(s0r + (((s1r - s0r) * (int)fr8) >> 8)) * v.vol);
                    v.pos += v.step;
                }

                if (l > 32767) {
                    l = 32767;
                } else if (l < -32768) {
                    l = -32768;
                }

                if (r > 32767) {
                    r = 32767;
                } else if (r < -32768) {
                    r = -32768;
                }

                dst[f * 2u] = (short)l;
                dst[f * 2u + 1u] = (short)r;
            }
        }

        const int tickBytes = kFramesPerTick * kOutCh * (int)sizeof(short);

        if (SDL_GetAudioStreamQueued(s_audioStream) < kQueueTicks * tickBytes) {
            if (SDL_PutAudioStreamData(s_audioStream, dst, tickBytes)) {
                s_appendCount++;
            } else {
                s_appendFailCount++;
            }
        } else {
            s_appendFailCount++;
        }
    }

    /// @brief Mute UI voices and start queuing video PCM.
    void VideoBegin()
    {
        for (unsigned i = 0; i < kVoices; i++) {
            s_voices[i].on = false;
        }

        pthread_mutex_lock(&s_videoMutex);
        s_videoMode = true;
        s_videoPaused = false;
        s_videoHead = 0;
        s_videoLength = 0;
        pthread_mutex_unlock(&s_videoMutex);
        s_videoPlayed = 0;
    }

    /// @brief Stop video PCM mode and drain the video buffer.
    void VideoEnd()
    {
        pthread_mutex_lock(&s_videoMutex);
        s_videoMode = false;
        s_videoPaused = false;
        s_videoHead = 0;
        s_videoLength = 0;
        pthread_mutex_unlock(&s_videoMutex);
    }

    /// @brief Push interleaved stereo PCM into the video buffer.
    /// @param pcm Interleaved frames, or null to ignore.
    /// @param frames Frame count (stereo pairs).
    void VideoPush(const short *pcm, unsigned frames)
    {
        if (!pcm) {
            return;
        }

        pthread_mutex_lock(&s_videoMutex);

        for (unsigned i = 0; i < frames * 2u; i++) {
            if (s_videoLength >= kVidCap) {
                break;
            }

            s_videoBuffer[(s_videoHead + s_videoLength) % kVidCap] = pcm[i];
            s_videoLength++;
        }

        pthread_mutex_unlock(&s_videoMutex);
    }

    /// @brief Seek the video clock without pushing samples.
    /// @param secs New clock position in seconds.
    void VideoSeekTo(double secs)
    {
        pthread_mutex_lock(&s_videoMutex);
        s_videoHead = 0;
        s_videoLength = 0;
        pthread_mutex_unlock(&s_videoMutex);
        s_videoPlayed = secs > 0.0 ? (unsigned)(secs * 48000.0) : 0u;
    }

    /// @brief Whether the video buffer wants more PCM.
    /// @return True when below the refill threshold.
    bool VideoHungry()
    {
        pthread_mutex_lock(&s_videoMutex);
        bool hungry = s_videoLength < kVidHungry;
        pthread_mutex_unlock(&s_videoMutex);
        return hungry;
    }

    /// @brief Give buffered video audio in seconds.
    /// @return Buffered seconds (0 when empty).
    double VideoBuffered()
    {
        pthread_mutex_lock(&s_videoMutex);
        double secs = (double)s_videoLength / (48000.0 * 2.0);
        pthread_mutex_unlock(&s_videoMutex);
        return secs;
    }

    /// @brief Hold or release video PCM output (pause without draining).
    /// @param paused True to output silence and keep buffered PCM.
    void VideoSetPaused(bool paused)
    {
        pthread_mutex_lock(&s_videoMutex);
        s_videoPaused = paused;
        pthread_mutex_unlock(&s_videoMutex);
    }

    /// @brief Give the video clock in seconds.
    /// @return Seconds played.
    double VideoTime()
    {
        return (double)s_videoPlayed / 48000.0;
    }

} /* namespace Sfx */
