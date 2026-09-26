/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Sfx.hpp"
#include "Assets.hpp"
#include "Qext.hpp"
#include <string.h>
#include <stdlib.h>
#include <math.h>

namespace Sfx {

enum { OUT_RATE = 48000, OUT_CH = 2, FRAMES_PER_TICK = 800, POOL_N = 4, VOICES = 8 };

struct Clip {
    const short *pcm;
    unsigned frames;
    unsigned ch;
    unsigned rate;
    float gain;
};

struct Voice {
    const short *pcm;
    unsigned frames;
    unsigned ch;
    unsigned step;
    unsigned pos;
    float vol;
    bool on;
};

static Clip s_clips[Count];
static Voice s_voices[VOICES];
static unsigned s_cursor = 0;
static bool s_ready = false;

static short s_pool[POOL_N][2048] __attribute__((aligned(0x1000)));
static AudioOutBuffer s_buf[POOL_N];
static unsigned s_qi = 0;
static unsigned s_ticks = 0;
static unsigned s_appends = 0;
static unsigned s_appFails = 0;
static unsigned s_plays = 0;
static unsigned s_rng = 0x12345678;
static unsigned s_rels = 0;

static unsigned Rd32(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static bool ParseWav(const unsigned char *d, unsigned n, Clip &c)
{
    c.pcm = 0;
    c.frames = 0;
    c.ch = 0;
    c.rate = 0;
    if (!d || n < 44)
        return false;
    if (memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0)
        return false;
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
        if (body > n || sz > n)
            break;
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
    if (fmt != 1 || bits != 16 || ch < 1 || ch > 2 || !data || !dframes)
        return false;
    c.pcm = data;
    c.frames = dframes;
    c.ch = ch;
    c.rate = rate;
    return true;
}

static void LoadOne(Id id, const unsigned char *d, unsigned n, float db)
{
    Clip c;
    c.gain = 1.0f;
    if (ParseWav(d, n, c) && c.rate >= 8000 && c.rate <= 96000) {
        c.gain = powf(10.0f, db / 20.0f);
        s_clips[(int)id] = c;
        logging::LogLine("[sfx] clip %d: %u frames %u ch %u Hz", (int)id, c.frames, c.ch, c.rate);
    } else {
        logging::LogLine("[sfx] bad clip %d", (int)id);
    }
}

bool Init()
{
    if (s_ready)
        return true;
    memset(s_clips, 0, sizeof(s_clips));
    memset(s_voices, 0, sizeof(s_voices));
    memset(s_buf, 0, sizeof(s_buf));
    LoadOne(Hover, s2_cursor_move_wav, s2_cursor_move_wav_size, -7.0f);
    LoadOne(Click, s2_button_click_wav, s2_button_click_wav_size, 0.0f);
    LoadOne(Back, s2_back_select_wav, s2_back_select_wav_size, 0.0f);
    LoadOne(AlbumJ, s2_album_wav, s2_album_wav_size, -7.362f);
    LoadOne(SettingsJ, s2_settings_wav, s2_settings_wav_size, -5.0f);
    LoadOne(HomeJ, s2_home_menu_wav, s2_home_menu_wav_size, -10.464f);
    LoadOne(HomeBoot, s2_home_menu_wav, s2_home_menu_wav_size, -16.0f);
    LoadOne(EshopJ, s2_eshop_wav, s2_eshop_wav_size, -1.517f);
    LoadOne(ChatJ, s2_game_chat_wav, s2_game_chat_wav_size, -5.119f);
    LoadOne(FolderJ, news_wav, news_wav_size, 0.0f);
    LoadOne(VgcJ, virtual_game_cards_wav, virtual_game_cards_wav_size, 0.0f);
    LoadOne(StandbyJ, s2_standby_wav, s2_standby_wav_size, -20.0f);
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
        Result orc = audoutOpenAudioOut(want, devout, OUT_RATE, OUT_CH, &rateOut, &chOut, &fmtOut, &stOut);
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

void Shutdown()
{
    for (unsigned i = 0; i < VOICES; i++)
        s_voices[i].on = false;
    if (s_ready) {
        s_ready = false;
        audoutExit();
    }
}

void Play(Id id, float pitch)
{
    if (!s_ready || (int)id < 0 || (int)id >= (int)Count)
        return;
    const Clip &c = s_clips[(int)id];
    if (!c.pcm || !c.frames || !c.rate)
        return;
    Voice &v = s_voices[s_cursor];
    s_cursor = (s_cursor + 1u) % VOICES;
    v.pcm = c.pcm;
    v.frames = c.frames;
    v.ch = c.ch;
    float p = pitch;
    if (id == Hover) {
        s_rng = s_rng * 1664525u + 1013904223u + s_ticks;
        float u = (float)(s_rng >> 8) * (1.0f / 16777216.0f);
        p = powf(1.3f, u * 2.0f - 1.0f);
    }
    if (p < 0.25f)
        p = 0.25f;
    else if (p > 4.0f)
        p = 4.0f;
    v.step = (unsigned)((double)c.rate * (double)p * 65536.0 / (double)OUT_RATE);
    if (!v.step)
        v.step = 1;
    v.pos = 0;
    v.vol = c.gain;
    v.on = true;
    s_plays++;
}

void Tick()
{
    if (!s_ready)
        return;
    short *dst = s_pool[s_qi];
    for (unsigned f = 0; f < FRAMES_PER_TICK; f++) {
        int l = 0;
        int r = 0;
        for (unsigned vi = 0; vi < VOICES; vi++) {
            Voice &v = s_voices[vi];
            if (!v.on)
                continue;
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
        if (l > 32767)
            l = 32767;
        else if (l < -32768)
            l = -32768;
        if (r > 32767)
            r = 32767;
        else if (r < -32768)
            r = -32768;
        dst[f * 2u] = (short)l;
        dst[f * 2u + 1u] = (short)r;
    }
    AudioOutBuffer *ab = &s_buf[s_qi];
    s_qi = (s_qi + 1u) % POOL_N;
    ab->next = 0;
    ab->buffer = dst;
    ab->buffer_size = sizeof(s_pool[0]);
    ab->data_size = (u64)FRAMES_PER_TICK * 2u * 2u;
    ab->data_offset = 0;
    if (R_SUCCEEDED(audoutAppendAudioOutBuffer(ab))) {
        s_appends++;
    } else {
        for (unsigned k = 0; k < 64; k++) {
            AudioOutBuffer *rel = 0;
            u32 nrel = 0;
            if (R_FAILED(audoutGetReleasedAudioOutBuffer(&rel, &nrel)) || !nrel)
                break;
            s_rels++;
        }
        if (R_SUCCEEDED(audoutAppendAudioOutBuffer(ab)))
            s_appends++;
        else
            s_appFails++;
    }
    if (++s_ticks == 300)
        logging::LogLine("[sfx] 300 ticks: ok=%u fail=%u plays=%u rels=%u", s_appends, s_appFails, s_plays, s_rels);
}

} /* namespace Sfx */
