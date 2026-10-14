/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Sfx.hpp"
#include "Assets.hpp"
#include "Json.hpp"
#include "Qext.hpp"
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

        /// @brief Audio queue pool depth.
        constexpr int kPoolN = 4;

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

        /// @brief Embedded WAV blob reference for asset lookup.
        struct AssetRef {
            const char *name;
            const unsigned char *data;
            unsigned size;
        };

        /// @brief Clip registry backing store.
        RegClip s_reg[kMaxClips];

        /// @brief Number of live entries in s_reg.
        int s_nReg = 0;

        /// @brief Live mixer voices.
        Voice s_voices[kVoices];

        /// @brief Round-robin cursor into s_voices.
        unsigned s_cursor = 0;

        /// @brief True once audio output is open.
        bool s_ready = false;

        /// @brief Audio queue pool and bookkeeping.
        short s_pool[kPoolN][2048] __attribute__((aligned(0x1000)));

        /// @brief Audio queue descriptors.
        AudioOutBuffer s_buf[kPoolN];

        /// @brief Round-robin cursor into s_pool/s_buf.
        unsigned s_qi = 0;

        /// @brief Ticks elapsed (seeds hover jitter).
        unsigned s_ticks = 0;

        /// @brief Successful buffer appends.
        unsigned s_appends = 0;

        /// @brief Failed buffer appends.
        unsigned s_appFails = 0;

        /// @brief Total Play() calls accepted.
        unsigned s_plays = 0;

        /// @brief RNG state for hover pitch jitter.
        unsigned s_rng = 0x12345678;

        /// @brief Released buffers reclaimed.
        unsigned s_rels = 0;

        /// @brief True while video PCM mode mutes UI voices.
        bool s_video = false;

        /// @brief Video PCM ring buffer.
        short s_vidBuf[48000];

        /// @brief Read head into s_vidBuf.
        unsigned s_vidHead = 0;

        /// @brief Buffered (unplayed) sample count.
        unsigned s_vidLen = 0;

        /// @brief Total video samples queued since begin/seek.
        unsigned s_vidPlayed = 0;

        /// @brief Legacy Id-to-name mapping for Play(Id) compatibility.
        const char *const kLegacyNames[Count] = {
            "hover", "click", "back", "album_jingle", "settings_jingle",
            "home_jingle", "home_boot", "eshop_jingle", "chat_jingle",
            "folder_jingle", "vgc_jingle", "standby_jingle",
        };

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

        /// @brief Resolve an asset name to its embedded WAV blob.
        /// @param name Asset name from sfx.json (for example "s2_album").
        /// @param data Output blob pointer.
        /// @param size Output blob size.
        /// @return True when the asset exists.
        bool FindAsset(const char *name, const unsigned char **data, unsigned *size)
        {
            static const AssetRef kAssets[] = {
                { "s2_cursor_move", s2_cursor_move_wav, 0 },
                { "s2_button_click", s2_button_click_wav, 0 },
                { "s2_back_select", s2_back_select_wav, 0 },
                { "s2_album", s2_album_wav, 0 },
                { "s2_settings", s2_settings_wav, 0 },
                { "s2_home_menu", s2_home_menu_wav, 0 },
                { "s2_eshop", s2_eshop_wav, 0 },
                { "s2_game_chat", s2_game_chat_wav, 0 },
                { "s2_standby", s2_standby_wav, 0 },
                { "news", news_wav, 0 },
                { "virtual_game_cards", virtual_game_cards_wav, 0 },
            };
            static const unsigned kAssetSizes[] = {
                s2_cursor_move_wav_size, s2_button_click_wav_size,
                s2_back_select_wav_size, s2_album_wav_size,
                s2_settings_wav_size, s2_home_menu_wav_size,
                s2_eshop_wav_size, s2_game_chat_wav_size,
                s2_standby_wav_size, news_wav_size,
                virtual_game_cards_wav_size,
            };

            if (!name || !name[0] || !data || !size) {
                return false;
            }

            for (unsigned i = 0; i < sizeof(kAssets) / sizeof(kAssets[0]); i++) {
                if (strcmp(kAssets[i].name, name) == 0) {
                    *data = kAssets[i].data;
                    *size = kAssetSizes[i];
                    return *data && *size;
                }
            }

            return false;
        }

        /// @brief Clear the clip registry.
        void ClearReg()
        {
            memset(s_reg, 0, sizeof(s_reg));
            s_nReg = 0;
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

            for (int i = 0; i < s_nReg; i++) {
                if (strncmp(s_reg[i].id, id, kIdMax) == 0) {
                    s_reg[i].clip = clip;
                    s_reg[i].jitter = jitter;
                    return i;
                }
            }

            if (s_nReg >= kMaxClips) {
                return -1;
            }

            RegClip *r = &s_reg[s_nReg];
            memset(r, 0, sizeof(*r));
            size_t len = strlen(id);

            if (len > (size_t)(kIdMax - 1)) {
                len = (size_t)(kIdMax - 1);
            }

            memcpy(r->id, id, len);
            r->id[len] = 0;
            r->clip = clip;
            r->jitter = jitter;
            return s_nReg++;
        }

        /// @brief Decode one asset blob and store it under a clip id.
        /// @param id Clip id.
        /// @param asset Asset name resolving via FindAsset().
        /// @param db Gain in decibels.
        /// @param jitter True to enable pitch shimmer.
        void LoadOneNamed(const char *id, const char *asset, float db, bool jitter)
        {
            const unsigned char *d = NULL;
            unsigned n = 0;

            if (!FindAsset(asset, &d, &n)) {
                logging::LogLine("[sfx] unknown asset '%s' for clip '%s'", asset ? asset : "?", id ? id : "?");
                return;
            }

            Clip c;
            c.gain = 1.0f;

            if (ParseWav(d, n, c) && c.rate >= 8000 && c.rate <= 96000) {
                c.gain = powf(10.0f, db / 20.0f);

                if (StoreClip(id, c, jitter) >= 0) {
                    logging::LogLine("[sfx] clip '%s': %u frames %u ch %u Hz", id, c.frames, c.ch, c.rate);
                }
            } else {
                logging::LogLine("[sfx] bad clip '%s'", id ? id : "?");
            }
        }

        /// @brief Load the hardcoded legacy table (fallback path).
        void LoadLegacy()
        {
            ClearReg();
            LoadOneNamed("hover", "s2_cursor_move", -7.0f, true);
            LoadOneNamed("click", "s2_button_click", 0.0f, false);
            LoadOneNamed("back", "s2_back_select", 0.0f, false);
            LoadOneNamed("album_jingle", "s2_album", -7.362f, false);
            LoadOneNamed("settings_jingle", "s2_settings", -5.0f, false);
            LoadOneNamed("home_jingle", "s2_home_menu", -10.464f, false);
            LoadOneNamed("home_boot", "s2_home_menu", -16.0f, false);
            LoadOneNamed("eshop_jingle", "s2_eshop", -1.517f, false);
            LoadOneNamed("chat_jingle", "s2_game_chat", -5.119f, false);
            LoadOneNamed("folder_jingle", "news", 0.0f, false);
            LoadOneNamed("vgc_jingle", "virtual_game_cards", 0.0f, false);
            LoadOneNamed("standby_jingle", "s2_standby", -20.0f, false);
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
            if (!s_ready || slot < 0 || slot >= s_nReg) {
                return;
            }

            if (s_video) {
                return;
            }

            const RegClip &r = s_reg[slot];

            if (!r.clip.pcm || !r.clip.frames || !r.clip.rate) {
                return;
            }

            Voice &v = s_voices[s_cursor];
            s_cursor = (s_cursor + 1u) % kVoices;
            v.pcm = r.clip.pcm;
            v.frames = r.clip.frames;
            v.ch = r.clip.ch;

            float p = pitch;

            if (r.jitter) {
                s_rng = s_rng * 1664525u + 1013904223u + (s_ticks++);
                float u = (float)(s_rng >> 8) * (1.0f / 16777216.0f);
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
            s_plays++;
        }

    } // namespace

    /// @brief Initialize audio output and load the clip registry.
    /// @return True when audio is ready (registry still loads on failure).
    bool Init()
    {
        if (s_ready) {
            return true;
        }

        memset(s_voices, 0, sizeof(s_voices));
        memset(s_buf, 0, sizeof(s_buf));

        if (!LoadAuto()) {
            LoadLegacy();
        }

        Result rc = audoutInitialize();

        if (R_FAILED(rc)) {
            logging::LogLine("[sfx] audout rc=0x%X", (unsigned)rc);
            return false;
        }

        {
            static char devnames[4][256];
            u32 got = 0;
            Result lr = audoutListAudioOuts(&devnames[0][0], 4, &got);
            logging::LogLine("[sfx] list rc=0x%X got=%u", (unsigned)lr, got);
            char devout[256];
            memset(devout, 0, sizeof(devout));
            u32 rateOut = 0;
            u32 chOut = 0;
            PcmFormat fmtOut = PcmFormat_Invalid;
            AudioOutState stOut = AudioOutState_Stopped;
            const char *want = (R_SUCCEEDED(lr) && got > 0) ? devnames[0] : "";
            Result orc = audoutOpenAudioOut(want, devout, kOutRate, kOutCh, &rateOut, &chOut, &fmtOut, &stOut);
            logging::LogLine("[sfx] open rc=0x%X rate=%u ch=%u fmt=%d state=%d", (unsigned)orc, rateOut, chOut, (int)fmtOut, (int)stOut);
        }

        rc = audoutStartAudioOut();

        if (R_FAILED(rc)) {
            logging::LogLine("[sfx] start rc=0x%X", (unsigned)rc);
            audoutExit();
            return false;
        }

        audoutSetAudioOutVolume(1.0f);
        AudioOutState curSt = AudioOutState_Stopped;
        audoutGetAudioOutState(&curSt);
        logging::LogLine("[sfx] ready rate=%u ch=%u state=%d", audoutGetSampleRate(), audoutGetChannelCount(), (int)curSt);
        s_ready = true;
        return true;
    }

    /// @brief Shut down voices and audio output.
    void Shutdown()
    {
        for (unsigned i = 0; i < kVoices; i++) {
            s_voices[i].on = false;
        }

        if (s_ready) {
            s_ready = false;
            audoutExit();
        }
    }

    /// @brief Play a legacy clip id.
    /// @param id Legacy clip id.
    /// @param pitch Playback rate multiplier, clamped 0.25-4.0.
    void Play(Id id, float pitch)
    {
        if ((int)id < 0 || (int)id >= (int)Count) {
            return;
        }

        PlayByName(kLegacyNames[(int)id], pitch);
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

        for (int i = 0; i < s_nReg; i++) {
            if (strncmp(s_reg[i].id, name, kIdMax) == 0) {
                return i;
            }
        }

        return -1;
    }

    /// @brief Number of clips in the registry.
    /// @return Clip count.
    int ClipCount()
    {
        return s_nReg;
    }

    /// @brief Look up a clip id by slot.
    /// @param slot Registry slot from IdOf().
    /// @return Clip id string, or "" when out of range.
    const char *ClipName(int slot)
    {
        if (slot < 0 || slot >= s_nReg) {
            return "";
        }

        return s_reg[slot].id;
    }

    /// @brief Load the clip registry from a JSON buffer.
    /// @param json JSON text with a "clips" array.
    /// @param len Text length in bytes.
    /// @return True when at least one clip loaded.
    bool LoadJson(const char *json, unsigned len)
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
            LoadOneNamed(id, asset, db, jitter);
        }

        return s_nReg > 0;
    }

    /// @brief Load the embedded sfx.json registry.
    /// @return True on success.
    bool LoadBuiltin()
    {
        return LoadJson(sfx_json, sfx_json_size);
    }

    /// @brief Load a registry override from an SD file.
    /// @param path Filesystem path to sfx.json.
    /// @return True on success.
    bool LoadFile(const char *path)
    {
        FILE *f = fopen(path, "rb");

        if (!f) {
            return false;
        }

        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (sz <= 0 || sz > 65536) {
            fclose(f);
            return false;
        }

        char *buf = (char *)malloc((unsigned)sz);

        if (!buf) {
            fclose(f);
            return false;
        }

        size_t got = fread(buf, 1, (unsigned)sz, f);
        fclose(f);

        bool ok = (got == (unsigned)sz) && LoadJson(buf, (unsigned)sz);
        free(buf);
        return ok;
    }

    /// @brief Load the SD override, else the embedded registry.
    /// @return True on success.
    bool LoadAuto()
    {
        if (LoadFile("sdmc:/qlaunch-ext/sfx.json")) {
            return true;
        }

        return LoadBuiltin();
    }

    /// @brief Pump one mixer tick into the audio queue. Call every frame.
    void Tick()
    {
        if (!s_ready) {
            return;
        }

        short *dst = s_pool[s_qi];

        if (s_video) {
            for (unsigned f = 0; f < (unsigned)kFramesPerTick * 2u; f++) {
                if (s_vidLen > 0) {
                    dst[f] = s_vidBuf[s_vidHead];
                    s_vidHead = (s_vidHead + 1u) % 48000u;
                    s_vidLen--;
                } else {
                    dst[f] = 0;
                }
            }

            s_vidPlayed += kFramesPerTick;
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

        AudioOutBuffer *ab = &s_buf[s_qi];
        s_qi = (s_qi + 1u) % kPoolN;
        ab->next = 0;
        ab->buffer = dst;
        ab->buffer_size = sizeof(s_pool[0]);
        ab->data_size = (u64)kFramesPerTick * 2u * 2u;
        ab->data_offset = 0;

        if (R_SUCCEEDED(audoutAppendAudioOutBuffer(ab))) {
            s_appends++;
        } else {
            for (unsigned k = 0; k < 64; k++) {
                AudioOutBuffer *rel = 0;
                u32 nrel = 0;

                if (R_FAILED(audoutGetReleasedAudioOutBuffer(&rel, &nrel)) || !nrel) {
                    break;
                }

                s_rels++;
            }

            if (R_SUCCEEDED(audoutAppendAudioOutBuffer(ab))) {
                s_appends++;
            } else {
                s_appFails++;
            }
        }
    }

    /// @brief Mute UI voices and start queuing video PCM.
    void VideoBegin()
    {
        for (unsigned i = 0; i < kVoices; i++) {
            s_voices[i].on = false;
        }

        s_video = true;
        s_vidHead = 0;
        s_vidLen = 0;
        s_vidPlayed = 0;
    }

    /// @brief Stop video PCM mode and drain the video buffer.
    void VideoEnd()
    {
        s_video = false;
        s_vidHead = 0;
        s_vidLen = 0;
    }

    /// @brief Push interleaved stereo PCM into the video buffer.
    /// @param pcm Interleaved frames, or null to ignore.
    /// @param frames Frame count (stereo pairs).
    void VideoPush(const short *pcm, unsigned frames)
    {
        if (!pcm) {
            return;
        }

        for (unsigned i = 0; i < frames * 2u; i++) {
            if (s_vidLen >= 48000u) {
                break;
            }

            s_vidBuf[(s_vidHead + s_vidLen) % 48000u] = pcm[i];
            s_vidLen++;
        }
    }

    /// @brief Seek the video clock without pushing samples.
    /// @param secs New clock position in seconds.
    void VideoSeekTo(double secs)
    {
        s_vidHead = 0;
        s_vidLen = 0;
        s_vidPlayed = secs > 0.0 ? (unsigned)(secs * 48000.0) : 0u;
    }

    /// @brief Whether the video buffer wants more PCM.
    /// @return True when below the refill threshold.
    bool VideoHungry()
    {
        return s_vidLen < 9600u;
    }

    /// @brief Give the video clock in seconds.
    /// @return Seconds played.
    double VideoTime()
    {
        return (double)s_vidPlayed / 48000.0;
    }

} /* namespace Sfx */
