/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Sfx.hpp"
#include "../core/Qext.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/hwcontext.h>
}

namespace WVideo {

    constexpr int VW = 1280;
    constexpr int VH = 720;
    constexpr int AVIO_CAP = 65536;
    constexpr int PCM_CAP = 8192;
    constexpr int VQ_CAP = 240;

    static bool s_open = false;
    static bool s_fresh = false;
    static int s_rep = 0;
    static int s_idx = -1;

    static AVFormatContext *s_fmt = 0;
    static AVCodecContext *s_vctx = 0;
    static AVCodecContext *s_actx = 0;
    static int s_vstream = -1;
    static int s_astream = -1;
    static AVFrame *s_sframe = 0;
    static bool s_vhw = false;
    static bool s_xferLogged = false;
    static AVBufferRef *s_hwdev = 0;
    static SwsContext *s_sws = 0;
    static SwrContext *s_swr = 0;
    static AVIOContext *s_avio = 0;
    static uint8_t *s_avioBuf = 0;
    static AVFrame *s_frame = 0;
    static AVFrame *s_aframe = 0;
    static AVPacket *s_pkt = 0;

    static double s_dur = 0.0;
    static int64_t s_vstart = 0;
    static double s_vtb = 0.0;
    static int64_t s_astart = 0;
    static double s_atb = 0.0;

    /// @brief Per-reader AVIO cursor. Probe and playback each own one, so a
    /// duration probe never clobbers the playback position/size.
    struct AvioCtx {
        u64 pos;
        u64 size;
    };

    static AvioCtx s_io;

    static pthread_t s_thread;
    static bool s_threadRunning = false;
    static pthread_mutex_t s_mx = PTHREAD_MUTEX_INITIALIZER;
    static bool s_stop = false;
    static bool s_seekReq = false;
    static double s_seekTo = 0.0;
    static double s_clkBase = 0.0;
    static double s_pauseAt = 0.0;
    static u64 s_clkTick = 0;
    static bool s_paused = false;
    static bool s_ended = false;

    static AVPacket *s_vq[VQ_CAP];
    static int s_vqHead = 0;
    static int s_vqTail = 0;
    static int s_vqCount = 0;

    static int16_t *s_pcmBuf = 0;
    static uint8_t *s_rgb[2] = { 0, 0 };
    static int s_readyBuf = -1;
    static int s_shownBuf = -1;

    static int s_Texture = -1;

    /// @brief Monotonic time in nanoseconds.
    static u64 NowTicks()
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
    }

    /// @brief Sleep a few milliseconds
    static void SleepMs(int ms)
    {
        struct timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (long)(ms % 1000) * 1000000L;
        nanosleep(&ts, NULL);
    }

    /// @brief Playback clock
    static double ClockNow()
    {
        pthread_mutex_lock(&s_mx);
        double t;

        if (s_paused) {
            t = s_pauseAt;
        } else {
            t = s_clkBase + (double)(NowTicks() - s_clkTick) / 1e9;
        }

        pthread_mutex_unlock(&s_mx);
        return t;
    }

    /// @brief Reset the playback clock to a position.
    /// @param secs New position in seconds.
    static void ClockSet(double secs)
    {
        pthread_mutex_lock(&s_mx);
        s_clkBase = s_pauseAt = secs;
        s_clkTick = NowTicks();
        pthread_mutex_unlock(&s_mx);
    }

    /// @brief Freeze or resume the playback clock (and the audio output).
    /// @param paused True to freeze.
    static void SetPaused(bool paused)
    {
        pthread_mutex_lock(&s_mx);

        if (paused == s_paused) {
            pthread_mutex_unlock(&s_mx);
            return;
        }

        u64 now = NowTicks();

        if (paused) {
            s_pauseAt = s_clkBase + (double)(now - s_clkTick) / 1e9;
        } else {
            s_clkBase = s_pauseAt;
            s_clkTick = now;
        }

        s_paused = paused;
        pthread_mutex_unlock(&s_mx);
        Sfx::VideoSetPaused(paused);
    }

    /// @brief Whether the worker was asked to stop.
    /// @return True when stopping.
    static bool Stopped()
    {
        pthread_mutex_lock(&s_mx);
        bool r = s_stop;
        pthread_mutex_unlock(&s_mx);
        return r;
    }

    /// @brief Whether a seek is pending (worker peek, keeps the request).
    /// @return True when a seek is pending.
    static bool SeekPending()
    {
        pthread_mutex_lock(&s_mx);
        bool r = s_seekReq;
        pthread_mutex_unlock(&s_mx);
        return r;
    }

    /// @brief Take a pending seek request.
    /// @param out Target time output.
    /// @return True when a request was pending.
    static bool TakeSeek(double *out)
    {
        pthread_mutex_lock(&s_mx);
        bool r = s_seekReq;

        if (r) {
            *out = s_seekTo;
            s_seekReq = false;
            s_ended = false;
        }

        pthread_mutex_unlock(&s_mx);
        return r;
    }

    /// @brief FFmpeg read callback pulling from the album movie handle.
    /// @param opaque AvioCtx for this reader.
    /// @param buf Output buffer.
    /// @param size Bytes requested.
    /// @return Bytes read, or AVERROR_EOF.
    static int AvioRead(void *opaque, uint8_t *buf, int size)
    {
        AvioCtx *io = (AvioCtx *)opaque;

        if (!io || !buf || size <= 0) {
            return AVERROR_EOF;
        }

        int total = 0;

        while (total < size) {
            int got = qext_album_movie_read(io->pos, buf + total, (unsigned)(size - total));

            if (got <= 0) {
                break;
            }

            total += got;
            io->pos += (u64)got;
        }

        if (total == 0) {
            return AVERROR_EOF;
        }

        return total;
    }

    /// @brief FFmpeg seek callback over the album movie handle.
    /// @param opaque AvioCtx for this reader.
    /// @param offset Byte offset.
    /// @param whence Seek mode (or AVSEEK_SIZE for size query).
    /// @return New position, or the size for AVSEEK_SIZE.
    static int64_t AvioSeek(void *opaque, int64_t offset, int whence)
    {
        AvioCtx *io = (AvioCtx *)opaque;

        if (!io) {
            return AVERROR_EOF;
        }

        if (whence == AVSEEK_SIZE) {
            return (int64_t)io->size;
        }

        if (offset < 0) {
            offset = 0;
        }

        if ((u64)offset > io->size) {
            offset = (int64_t)io->size;
        }

        io->pos = (u64)offset;

        return offset;
    }

    /// @brief Free the video frame texture.
    static void FreeTexture()
    {
        Gfx::TexFree(s_Texture);
        s_Texture = -1;
    }

    /// @brief Allocate the fixed video frame texture.
    /// @return True on success.
    static bool AllocTexture()
    {
        void *px = nullptr;
        int pitch = 0;
        if (s_Texture >= 0) {
            return true;
        }

        FreeTexture();
        s_Texture = Gfx::TexAlloc();

        if (s_Texture < 0) {
            return false;
        }

        if (!Gfx::TexLock(s_Texture, VW, VH, &px, &pitch) || !px || pitch <= 0) {
            FreeTexture();
            return false;
        }

        Gfx::TexUnlock(s_Texture);
        return true;
    }

    /// @brief Free all FFmpeg contexts, the packet queue and IO buffers.
    static void Cleanup()
    {
        for (int i = 0; i < VQ_CAP; i++) {
            if (s_vq[i]) {
                av_packet_free(&s_vq[i]);
                s_vq[i] = 0;
            }
        }

        s_vqHead = s_vqTail = s_vqCount = 0;

        if (s_swr) {
            swr_free(&s_swr);
            s_swr = 0;
        }

        if (s_sws) {
            sws_freeContext(s_sws);
            s_sws = 0;
        }

        if (s_pkt) {
            av_packet_free(&s_pkt);
            s_pkt = 0;
        }

        if (s_frame) {
            av_frame_free(&s_frame);
            s_frame = 0;
        }

        if (s_aframe) {
            av_frame_free(&s_aframe);
            s_aframe = 0;
        }

        if (s_vctx) {
            avcodec_free_context(&s_vctx);
            s_vctx = 0;
        }

        if (s_actx) {
            avcodec_free_context(&s_actx);
            s_actx = 0;
        }

        if (s_fmt) {
            avformat_close_input(&s_fmt);
            s_fmt = 0;
        }

        if (s_avio) {
            av_free(s_avio->buffer);
            avio_context_free(&s_avio);
            s_avioBuf = 0;
        }

        s_vstream = -1;
        s_astream = -1;
        s_vhw = false;
    }

    /// @brief Close the album movie handle without touching decoders.
    static void DropHandle()
    {
        qext_album_movie_close();
    }

    /// @brief Ensure the NVTEGRA hardware device exists.
    /// @return True when ready.
    static bool EnsureHwDev()
    {
        if (s_hwdev) {
            return true;
        }

        if (av_hwdevice_ctx_create(&s_hwdev, AV_HWDEVICE_TYPE_NVTEGRA, NULL, NULL, 0) != 0) {
            s_hwdev = 0;
            logging::LogLine("[video] Failed to init hw device");
            return false;
        }

        return true;
    }

    /// @brief Pick an HW-friendly pixel format for the decoder.
    /// @param ctx Codec context (unused).
    /// @param fmts Candidate formats.
    /// @return Chosen format.
    static enum AVPixelFormat HwGetFormat(AVCodecContext *ctx, const enum AVPixelFormat *fmts)
    {
        (void)ctx;

        for (const enum AVPixelFormat *f = fmts; *f != AV_PIX_FMT_NONE; f++) {
            if (*f == AV_PIX_FMT_NVTEGRA) {
                return AV_PIX_FMT_NVTEGRA;
            }
        }

        return fmts[0];
    }

    /// @brief Open a decoder for one stream, with HW accel for H264.
    /// @param streamIdx Stream index in s_fmt.
    /// @return Codec context, or null on failure.
    static AVCodecContext *OpenDecoder(int streamIdx)
    {
        if (!s_fmt || streamIdx < 0) {
            return 0;
        }

        AVStream *st = s_fmt->streams[streamIdx];
        const AVCodec *dec = avcodec_find_decoder(st->codecpar->codec_id);

        if (!dec) {
            logging::LogLine("[video] Failed to find decoder (id: %d)", (int)st->codecpar->codec_id);
            return 0;
        }

        AVCodecContext *ctx = avcodec_alloc_context3(dec);

        if (!ctx) {
            logging::LogLine("[video] Failed to alloc decoder (id: %d)", (int)st->codecpar->codec_id);
            return 0;
        }

        int prc = avcodec_parameters_to_context(ctx, st->codecpar);

        if (prc < 0) {
            logging::LogLine("[video] Failed to setup decoder (rc: %d)", prc);
            avcodec_free_context(&ctx);
            return 0;
        }

        ctx->thread_count = 1;

        if (st->codecpar->codec_id == AV_CODEC_ID_H264) {
            ctx->sw_pix_fmt = AV_PIX_FMT_NV12;

            if (EnsureHwDev()) {
                ctx->hw_device_ctx = av_buffer_ref(s_hwdev);

                if (ctx->hw_device_ctx) {
                    ctx->get_format = HwGetFormat;
                }
            }
        }

        int orc = avcodec_open2(ctx, dec, NULL);

        if (orc < 0) {
            logging::LogLine("[video] Failed to open decoder (rc: %d)", orc);
            avcodec_free_context(&ctx);
            return 0;
        }

        if (st->codecpar->codec_id == AV_CODEC_ID_H264 && ctx->hw_device_ctx) {
            s_vhw = true;
        }

        return ctx;
    }

    /// @brief Build sampler for audio decoder
    /// @return True when ready.
    static bool SetupResampler()
    {
        if (s_swr) {
            swr_free(&s_swr);
            s_swr = 0;
        }

        if (!s_actx) {
            return false;
        }

        AVChannelLayout outL = AV_CHANNEL_LAYOUT_STEREO;
        SwrContext *swr = 0;
        int rc = swr_alloc_set_opts2(&swr, &outL, AV_SAMPLE_FMT_S16, 48000,
                                     &s_actx->ch_layout, s_actx->sample_fmt,
                                     s_actx->sample_rate, 0, NULL);

        if (rc == 0) {
            rc = swr_init(swr);
        }

        if (rc != 0) {
            swr_free(&swr);
            return false;
        }

        s_swr = swr;
        return true;
    }

    /// @brief Worker side of a seek: land on the keyframe at or before the
    /// target, then decode forward silently so the first picture and sound
    /// are exactly at it (sLaunch DoSeek).
    /// @param seconds Target time in seconds.
    static void DoSeek(double seconds)
    {
        if (!s_fmt || s_vstream < 0) {
            return;
        }

        if (s_dur > 0.0) {
            if (seconds < 0.0) {
                seconds = 0.0;
            }

            if (seconds > s_dur - 0.1) {
                seconds = s_dur - 0.1;
            }

            if (seconds < 0.0) {
                seconds = 0.0;
            }
        } else if (seconds < 0.0) {
            seconds = 0.0;
        }

        AVStream *vs = s_fmt->streams[s_vstream];
        int64_t ts = s_vstart + (int64_t)(seconds / (s_vtb > 0.0 ? s_vtb : 1.0 / 30.0));
        av_seek_frame(s_fmt, s_vstream, ts, AVSEEK_FLAG_BACKWARD);

        if (s_vctx) {
            avcodec_flush_buffers(s_vctx);
        }

        if (s_actx) {
            avcodec_flush_buffers(s_actx);
            SetupResampler();
        }

        for (int i = 0; i < VQ_CAP; i++) {
            if (s_vq[i]) {
                av_packet_free(&s_vq[i]);
                s_vq[i] = 0;
            }
        }

        s_vqHead = s_vqTail = s_vqCount = 0;

        if (s_frame) {
            av_frame_unref(s_frame);
        }

        if (s_aframe) {
            av_frame_unref(s_aframe);
        }

        if (s_sframe) {
            av_frame_unref(s_sframe);
        }

        Sfx::VideoSeekTo(seconds);
        ClockSet(seconds);

        pthread_mutex_lock(&s_mx);
        s_ended = false;
        pthread_mutex_unlock(&s_mx);
    }

    /// @brief Decode one demuxed audio packet into the SFX ring.
    /// @param pkt Demuxed audio packet (borrowed).
    static void DecodeAudio(AVPacket *pkt, double skipUntil)
    {
        if (!s_actx || !s_swr || !pkt) {
            return;
        }

        if (avcodec_send_packet(s_actx, pkt) < 0) {
            return;
        }

        while (!Stopped() && !SeekPending() && avcodec_receive_frame(s_actx, s_aframe) == 0) {
            int n = s_aframe->nb_samples;
            int rate = s_aframe->sample_rate > 0 ? s_aframe->sample_rate : 48000;
            int64_t ts = s_aframe->best_effort_timestamp;

            double pts = skipUntil;

            if (ts != AV_NOPTS_VALUE && s_atb > 0.0) {
                pts = (double)(ts - s_astart) * s_atb;
            }

            if (n <= 0 || pts + (double)n / (double)rate < skipUntil) {
                av_frame_unref(s_aframe);
                continue;
            }

            uint8_t *out[1] = { (uint8_t *)s_pcmBuf };
            int got = swr_convert(s_swr, out, PCM_CAP,
                                  (const uint8_t **)s_aframe->extended_data, n);
            av_frame_unref(s_aframe);

            if (got > 0) {
                Sfx::VideoPush(s_pcmBuf, (unsigned)got);
            }
        }

        av_frame_unref(s_aframe);
    }

    /// @brief Decode worker
    static void DecodeLoop()
    {
        double skipUntil = 0.0;
        bool readEof = false;
        bool decEof = false;
        bool drainSent = false;
        bool haveFrame = false;
        double framePts = 0.0;
        int writeBuf = 0;

        while (!Stopped()) {
            double seekTo = 0.0;

            if (TakeSeek(&seekTo)) {
                readEof = decEof = drainSent = false;
                haveFrame = false;
                DoSeek(seekTo);
                skipUntil = seekTo;
            }

            /* Read ahead */
            while (!readEof && !Stopped() && !SeekPending() && s_vqCount < VQ_CAP &&
                   (Sfx::VideoBuffered() < 0.6 || (s_vqCount < 6 && !haveFrame))) {
                if (av_read_frame(s_fmt, s_pkt) < 0) {
                    readEof = true;
                    break;
                }

                if (s_pkt->stream_index == s_astream && s_actx && s_swr) {
                    DecodeAudio(s_pkt, skipUntil);
                    av_packet_unref(s_pkt);
                } else if (s_pkt->stream_index == s_vstream) {
                    AVPacket *q = av_packet_alloc();

                    if (q) {
                        av_packet_move_ref(q, s_pkt);
                        s_vq[s_vqTail] = q;
                        s_vqTail = (s_vqTail + 1) % VQ_CAP;
                        s_vqCount++;
                    }

                    av_packet_unref(s_pkt);
                } else {
                    av_packet_unref(s_pkt);
                }
            }

            /* Next picture. */
            while (!haveFrame && !decEof && !Stopped() && !SeekPending()) {
                int r = avcodec_receive_frame(s_vctx, s_frame);

                if (r == 0) {
                    int64_t ts = s_frame->best_effort_timestamp;

                    if (ts == AV_NOPTS_VALUE) {
                        ts = s_frame->pts;
                    }

                    framePts = (ts != AV_NOPTS_VALUE && s_vtb > 0.0)
                        ? (double)(ts - s_vstart) * s_vtb
                        : ClockNow();

                    if (framePts < skipUntil - 0.001) {
                        av_frame_unref(s_frame);
                        continue;
                    }

                    haveFrame = true;
                } else if (r == AVERROR_EOF) {
                    decEof = true;
                } else if (s_vqCount > 0) {
                    AVPacket *q = s_vq[s_vqHead];
                    s_vq[s_vqHead] = 0;
                    s_vqHead = (s_vqHead + 1) % VQ_CAP;
                    s_vqCount--;
                    avcodec_send_packet(s_vctx, q);
                    av_packet_free(&q);
                } else if (readEof && !drainSent) {
                    avcodec_send_packet(s_vctx, NULL);
                    drainSent = true;
                } else {
                    break;
                }
            }

            if (haveFrame) {
                if (ClockNow() < framePts) {
                    SleepMs(2);
                    continue;
                }

                AVFrame *src = s_frame;

                if (s_frame->format == AV_PIX_FMT_NVTEGRA) {
                    if (av_hwframe_transfer_data(s_sframe, s_frame, 0) == 0) {
                        src = s_sframe;
                    } else {
                        if (!s_xferLogged) {
                            logging::LogLine("[video] Failed to download hw frame");
                            s_xferLogged = true;
                        }

                        av_frame_unref(s_frame);
                        av_frame_unref(s_sframe);
                        haveFrame = false;
                        continue;
                    }
                }

                s_sws = sws_getCachedContext(s_sws, src->width, src->height,
                                             (AVPixelFormat)src->format,
                                             VW, VH, AV_PIX_FMT_RGBA,
                                             SWS_BILINEAR, NULL, NULL, NULL);

                if (!s_sws || !s_rgb[writeBuf]) {
                    if (!s_sws) {
                        logging::LogLine("[video] Failed to alloc scaler");
                    }

                    av_frame_unref(s_frame);
                    av_frame_unref(s_sframe);
                    haveFrame = false;
                    continue;
                }

                uint8_t *dst[4] = { s_rgb[writeBuf], 0, 0, 0 };
                int stride[4] = { VW * 4, 0, 0, 0 };
                sws_scale(s_sws, src->data, src->linesize, 0, src->height, dst, stride);
                av_frame_unref(s_frame);
                av_frame_unref(s_sframe);
                haveFrame = false;

                __atomic_store_n(&s_readyBuf, writeBuf, __ATOMIC_RELEASE);
                writeBuf ^= 1;
                continue;
            }

            if (readEof && s_vqCount == 0 && (decEof || drainSent)) {
                if (ClockNow() >= s_dur - 0.05) {
                    pthread_mutex_lock(&s_mx);
                    s_ended = true;
                    pthread_mutex_unlock(&s_mx);
                    SetPaused(true);
                }
            }

            SleepMs(5);
        }

        for (int i = 0; i < s_vqCount; i++) {
            int at = (s_vqHead + i) % VQ_CAP;
            av_packet_free(&s_vq[at]);
            s_vq[at] = 0;
        }

        s_vqHead = s_vqTail = s_vqCount = 0;

        if (haveFrame) {
            av_frame_unref(s_frame);
        }
    }

    /// @brief Worker thread entry.
    /// @param arg Unused.
    /// @return Null.
    static void *DecodeEntry(void *arg)
    {
        (void)arg;
        DecodeLoop();
        return NULL;
    }

    /// @brief Open a movie entry for playback.
    /// @param albumIndex Album entry index.
    /// @return True on success.
    bool Open(int albumIndex)
    {
        Close();

        if (albumIndex < 0 || qext_album_is_movie(albumIndex) <= 0) {
            logging::LogLine("[video] Failed to open video (idx: %d)", albumIndex);
            return false;
        }

        if (!qext_album_movie_open(albumIndex)) {
            logging::LogLine("[video] Failed to open video (idx: %d)", albumIndex);
            return false;
        }

        u64 size = qext_album_movie_size();

        if (size == 0) {
            logging::LogLine("[video] Failed to get size (idx: %d)", albumIndex);
            qext_album_movie_close();
            return false;
        }

        s_io.pos = 0;
        s_io.size = size;
        s_idx = albumIndex;

        WAlbum::DropFull();
        WAlbum::DropThumbs();

        if (!s_pcmBuf) {
            s_pcmBuf = (int16_t *)malloc((size_t)PCM_CAP * 2u * sizeof(int16_t));
        }

        for (int i = 0; i < 2; i++) {
            if (!s_rgb[i]) {
                s_rgb[i] = (uint8_t *)malloc((size_t)VW * (size_t)VH * 4u);
            }
        }

        s_avioBuf = (uint8_t *)av_malloc(AVIO_CAP);

        s_frame = av_frame_alloc();
        s_aframe = av_frame_alloc();

        if (!s_sframe) {
            s_sframe = av_frame_alloc();
        }

        s_pkt = av_packet_alloc();

        if (!s_pcmBuf || !s_rgb[0] || !s_rgb[1] || !s_avioBuf ||
            !s_frame || !s_aframe || !s_sframe || !s_pkt) {
            logging::LogLine("[video] Failed to alloc buffers");
            DropHandle();
            Cleanup();
            return false;
        }

        s_avio = avio_alloc_context(s_avioBuf, AVIO_CAP, 0, &s_io, AvioRead, NULL, AvioSeek);

        if (!s_avio) {
            logging::LogLine("[video] Failed to alloc avio");
            DropHandle();
            Cleanup();
            return false;
        }

        s_fmt = avformat_alloc_context();

        if (!s_fmt) {
            logging::LogLine("[video] Failed to alloc format");
            DropHandle();
            Cleanup();
            return false;
        }

        s_fmt->pb = s_avio;
        s_fmt->probesize = 256 * 1024;
        s_fmt->max_analyze_duration = 0;

        int orc = avformat_open_input(&s_fmt, NULL, NULL, NULL);

        if (orc < 0) {
            logging::LogLine("[video] Failed to open input (rc: %d)", orc);
            avformat_close_input(&s_fmt);
            DropHandle();
            Cleanup();
            return false;
        }

        int frc = avformat_find_stream_info(s_fmt, NULL);

        if (frc < 0) {
            logging::LogLine("[video] Failed to read info (rc: %d)", frc);
            DropHandle();
            Cleanup();
            return false;
        }

        s_vstream = av_find_best_stream(s_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

        if (s_vstream < 0) {
            logging::LogLine("[video] Failed to find video");
            DropHandle();
            Cleanup();
            return false;
        }

        s_vctx = OpenDecoder(s_vstream);

        if (!s_vctx) {
            logging::LogLine("[video] Failed to open video decoder");
            DropHandle();
            Cleanup();
            return false;
        }

        s_astream = av_find_best_stream(s_fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

        if (s_astream >= 0) {
            s_actx = OpenDecoder(s_astream);

            if (s_actx) {
                if (!SetupResampler()) {
                    logging::LogLine("[video] Failed to init audio");
                    swr_free(&s_swr);
                    s_swr = 0;
                    avcodec_free_context(&s_actx);
                    s_actx = 0;
                    s_astream = -1;
                }
            } else {
                s_astream = -1;
            }
        }

        AVCodecParameters *vp = s_fmt->streams[s_vstream]->codecpar;

        if (vp->width <= 0 || vp->height <= 0) {
            logging::LogLine("[video] Failed to read dims (w: %d h: %d)", vp->width, vp->height);
            DropHandle();
            Cleanup();
            return false;
        }

        s_sframe->format = AV_PIX_FMT_NV12;
        s_sframe->width = vp->width;
        s_sframe->height = vp->height;

        if (!AllocTexture()) {
            logging::LogLine("[video] Failed to alloc texture");
            DropHandle();
            Cleanup();
            return false;
        }

        s_dur = 0.0;

        if (s_fmt->duration != AV_NOPTS_VALUE) {
            s_dur = s_fmt->duration / (double)AV_TIME_BASE;
        }

        AVStream *vst = s_fmt->streams[s_vstream];
        s_vstart = (vst->start_time != AV_NOPTS_VALUE) ? vst->start_time : 0;
        s_vtb = av_q2d(vst->time_base);

        if (s_astream >= 0) {
            AVStream *ast = s_fmt->streams[s_astream];
            s_astart = (ast->start_time != AV_NOPTS_VALUE) ? ast->start_time : 0;
            s_atb = av_q2d(ast->time_base);
        } else {
            s_astart = 0;
            s_atb = 0.0;
        }

        s_xferLogged = false;
        Sfx::VideoBegin();
        pthread_mutex_lock(&s_mx);
        s_stop = false;
        s_seekReq = false;
        s_seekTo = 0.0;
        s_ended = false;
        pthread_mutex_unlock(&s_mx);
        s_readyBuf = -1;
        s_shownBuf = -1;
        ClockSet(0.0);
        SetPaused(false);

        if (pthread_create(&s_thread, NULL, DecodeEntry, NULL) != 0) {
            logging::LogLine("[video] Failed to start worker");
            DropHandle();
            Cleanup();
            Sfx::VideoEnd();
            s_idx = -1;
            return false;
        }

        s_threadRunning = true;
        s_open = true;
        s_fresh = true;
        s_rep = 0;

        return true;
    }

    /// @brief Seek to an absolute timestamp (handled by the worker).
    /// @param t Target time in seconds.
    static void SeekAbs(double t)
    {
        if (!s_open) {
            return;
        }

        if (s_dur > 0.0) {
            if (t < 0.0) {
                t = 0.0;
            }

            if (t > s_dur - 0.1) {
                t = s_dur - 0.1;
            }

            if (t < 0.0) {
                t = 0.0;
            }
        } else if (t < 0.0) {
            t = 0.0;
        }

        pthread_mutex_lock(&s_mx);
        s_seekTo = t;
        s_seekReq = true;
        s_ended = false;
        pthread_mutex_unlock(&s_mx);
    }

    /// @brief Step to a neighboring album entry, keeping video or image mode.
    /// @param delta Signed step.
    /// @return True when the step was handled.
    static bool StepContent(int delta)
    {
        int n = qext_album_count();
        int i = s_idx + delta;

        if (i < 0 || i >= n) {
            return false;
        }

        if (qext_album_is_movie(i) <= 0) {
            Close();
            WAlbum::OpenFull(i);
            return true;
        }

        int cur = s_idx;

        if (Open(i)) {
            return true;
        }

        Open(cur);

        return true;
    }

    /// @brief Whether a video is currently open.
    /// @return True when open.
    bool IsOpen()
    {
        return s_open;
    }

    /// @brief Whether playback is paused.
    /// @return True when paused.
    bool Paused()
    {
        pthread_mutex_lock(&s_mx);
        bool r = s_paused;
        pthread_mutex_unlock(&s_mx);
        return r;
    }

    /// @brief Whether a non-looping clip reached its end.
    /// @return True when ended.
    bool Ended()
    {
        pthread_mutex_lock(&s_mx);
        bool r = s_ended;
        pthread_mutex_unlock(&s_mx);
        return r;
    }

    /// @brief Give the playback position in seconds.
    /// @return Seconds from the start, clamped to the duration.
    double Position()
    {
        if (!s_open) {
            return 0.0;
        }

        double p = ClockNow();

        if (p < 0.0) {
            p = 0.0;
        }

        if (s_dur > 0.0 && p > s_dur) {
            p = s_dur;
        }

        return p;
    }

    /// @brief Close the video player.
    void Close()
    {
        if (!s_open) {
            return;
        }

        s_open = false;

        pthread_mutex_lock(&s_mx);
        s_stop = true;
        pthread_mutex_unlock(&s_mx);

        if (s_threadRunning) {
            pthread_join(s_thread, NULL);
            s_threadRunning = false;
        }

        if (s_vctx) {
            av_frame_unref(s_frame);
        }

        if (s_actx) {
            av_frame_unref(s_aframe);
        }

        Cleanup();
        DropHandle();
        Sfx::VideoEnd();
        s_idx = -1;

        pthread_mutex_lock(&s_mx);
        s_paused = false;
        s_ended = false;
        pthread_mutex_unlock(&s_mx);
        s_readyBuf = -1;
        s_shownBuf = -1;
    }

    /// @brief Drop pinned video textures.
    void DropPinned()
    {
        if (s_open) {
            Close();
        }

        FreeTexture();

        if (s_pcmBuf) {
            free(s_pcmBuf);
            s_pcmBuf = 0;
        }

        for (int i = 0; i < 2; i++) {
            if (s_rgb[i]) {
                free(s_rgb[i]);
                s_rgb[i] = 0;
            }
        }

        if (s_avioBuf) {
            av_free(s_avioBuf);
            s_avioBuf = 0;
        }

        if (s_sframe) {
            av_frame_free(&s_sframe);
            s_sframe = 0;
        }
    }

    /// @brief Put new frames on GPU. Call every frame.
    void Tick()
    {
        if (!s_open || s_Texture < 0) {
            return;
        }

        int ready = __atomic_load_n(&s_readyBuf, __ATOMIC_ACQUIRE);

        if (ready < 0 || ready == s_shownBuf || !s_rgb[ready]) {
            return;
        }

        void *locked = nullptr;
        int pitch = 0;

        if (!Gfx::TexLock(s_Texture, VW, VH, &locked, &pitch) || !locked || pitch <= 0) {
            return;
        }

        const uint8_t *src = s_rgb[ready];
        uint8_t *dst = (uint8_t *)locked;

        for (int y = 0; y < VH; y++) {
            memcpy(dst + (size_t)y * (size_t)pitch, src + (size_t)y * (size_t)VW * 4u,
                   (size_t)VW * 4u);
        }

        Gfx::TexUnlock(s_Texture);
        s_shownBuf = ready;
    }

    /// @brief Draw the current video frame and OSD.
    void Draw()
    {
        if (!s_open) {
            return;
        }

        Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

        if (s_Texture >= 0) {
            Gfx::PushIcon(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, s_Texture, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
        }
    }

    /// @brief Handle video playback input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    /// @return True when the input was consumed.
    bool Input(u64 down, u64 held)
    {
        (void)held;

        if (!s_open) {
            return false;
        }

        if (s_fresh) {
            s_fresh = false;
            return true;
        }

        u64 seeks = HidNpadButton_L | HidNpadButton_R;

        if (down & seeks) {
            s_rep = 0;
        }
        else if (held & seeks) {
            s_rep++;

            if (s_rep > 20 && (s_rep % 6) == 0) {
                down |= held & seeks;
            }
        }
        else {
            s_rep = 0;
        }

        if (down & HidNpadButton_B) {
            Close();
            Sfx::Play(Sfx::Back);
            return true;
        }

        if (down & HidNpadButton_A) {
            if (Ended()) {
                SeekAbs(0.0);
                SetPaused(false);
            }
            else {
                SetPaused(!Paused());
            }

            return true;
        }

        if (down & HidNpadButton_L) {
            SeekAbs(Position() - 5.0);
            return true;
        }

        if (down & HidNpadButton_R) {
            SeekAbs(Position() + 5.0);
            return true;
        }

        if (down & HidNpadButton_ZL) {
            StepContent(-1);
            return true;
        }

        if (down & HidNpadButton_ZR) {
            StepContent(1);
            return true;
        }

        if (down & (HidNpadButton_AnyLeft | HidNpadButton_AnyRight)) {
            if (down & HidNpadButton_AnyLeft) {
                StepContent(-1);
            }
            else {
                StepContent(1);
            }

            return true;
        }

        return true;
    }

    static double s_durCache[256];
    static bool s_durInit = false;

    /// @brief Probe a movie's duration without disturbing playback.
    /// @param albumIndex Album entry index.
    /// @return Duration in seconds, or negative on failure.
    static double ProbeDuration(int albumIndex)
    {
        bool wasOpen = s_open;
        int keepIdx = s_idx;

        if (!qext_album_movie_open(albumIndex)) {
            return -1.0;
        }

        u64 size = qext_album_movie_size();

        if (size == 0) {
            qext_album_movie_close();

            if (wasOpen && keepIdx >= 0) {
                qext_album_movie_open(keepIdx);
            }

            return -1.0;
        }

        AvioCtx io;
        io.pos = 0;
        io.size = size;

        double dur = -1.0;
        uint8_t *buf = (uint8_t *)av_malloc(AVIO_CAP);
        AVIOContext *avio = 0;

        if (buf) {
            avio = avio_alloc_context(buf, AVIO_CAP, 0, &io, AvioRead, NULL, AvioSeek);
        }

        if (avio) {
            AVFormatContext *fmt = avformat_alloc_context();

            if (fmt) {
                fmt->pb = avio;
                fmt->probesize = 256 * 1024;
                fmt->max_analyze_duration = 0;

                if (avformat_open_input(&fmt, NULL, NULL, NULL) == 0) {
                    if (avformat_find_stream_info(fmt, NULL) == 0 && fmt->duration != AV_NOPTS_VALUE) {
                        dur = fmt->duration / (double)AV_TIME_BASE;
                    }

                    avformat_close_input(&fmt);
                }
                else {
                    avformat_close_input(&fmt);
                }

                av_free(avio->buffer);
            }

            avio_context_free(&avio);
            avio = 0;
            buf = 0;
        }

        qext_album_movie_close();

        if (wasOpen && keepIdx >= 0) {
            qext_album_movie_open(keepIdx);
        }

        return dur;
    }

    /// @brief Probe (and cache) a movie's duration.
    /// @param albumIndex Album entry index.
    /// @return Duration in seconds, or 0 when unknown.
    double Duration(int albumIndex)
    {
        if (albumIndex < 0 || albumIndex >= 256) {
            return -1.0;
        }

        if (s_open && albumIndex == s_idx) {
            return s_dur;
        }

        if (!s_durInit) {
            for (int i = 0; i < 256; i++) {
                s_durCache[i] = -1.0;
            }

            s_durInit = true;
        }

        double c = s_durCache[albumIndex];

        if (c >= 0.0) {
            return c;
        }

        if (c < -1.5) {
            return -1.0;
        }

        double d = ProbeDuration(albumIndex);

        if (d > 0.0) {
            s_durCache[albumIndex] = d;
        }
        else {
            s_durCache[albumIndex] = -2.0;
        }

        return d;
    }

    /// @brief Clear cached durations.
    void DurReset()
    {
        s_durInit = false;
    }

} // namespace WVideo
