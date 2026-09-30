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

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavcodec/bsf.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
    #include <libavutil/opt.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/hwcontext.h>
}

namespace WVideo {

    enum { VW = 1280, VH = 720, AVIO_CAP = 65536 };
    enum { PCM_CAP = 8192 };

    static bool s_open = false;
    static bool s_paused = false;
    static bool s_ended = false;
    static bool s_fresh = false;
    static int s_rep = 0;
    static int s_idx = -1;
    static u64 s_size = 0;
    static u64 s_avioPos = 0;

    static AVFormatContext *s_fmt = 0;
    static AVCodecContext *s_vctx = 0;
    static AVCodecContext *s_actx = 0;
    static int s_vstream = -1;
    static int s_astream = -1;
    static AVBSFContext *s_vbsf = 0;
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

    static uint32_t s_texPitch = 0;
    static int16_t *s_pcmBuf = 0;
    static bool s_haveFrame = false;
    static double s_framePts = 0.0;
    static double s_fps = 30.0;
    static double s_dur = 0.0;
    static bool s_readEos = false;
    static bool s_vFlushed = false;
    static bool s_aFlushed = false;
    static bool s_eosV = false;
    static bool s_eosA = false;
    static void Step(bool force);

    static DkMemBlock s_mem = 0;
    static DkImage s_img;
    static int s_tex = -1;

    static int AvioRead(void *opaque, uint8_t *buf, int size)
    {
        (void)opaque;

        if (!buf || size <= 0) {
            return AVERROR_EOF;
        }

        int total = 0;

        while (total < size) {
            int got = qext_album_movie_read(s_avioPos, buf + total, (unsigned)(size - total));

            if (got <= 0) {
                break;
            }

            total += got;
            s_avioPos += (u64)got;
        }

        if (total == 0) {
            return AVERROR_EOF;
        }

        return total;
    }

    static int64_t AvioSeek(void *opaque, int64_t offset, int whence)
    {
        (void)opaque;

        if (whence == AVSEEK_SIZE) {
            return (int64_t)s_size;
        }

        if (offset < 0) {
            offset = 0;
        }

        if ((u64)offset > s_size) {
            offset = (int64_t)s_size;
        }

        s_avioPos = (u64)offset;

        return offset;
    }

    static void FreeTexture()
    {
        if (s_mem) {
            Gfx::WaitIdle();
            dkMemBlockDestroy(s_mem);
            s_mem = 0;
        }

        Gfx::TexFree(s_tex);
        s_tex = -1;
    }

    static bool AllocTexture()
    {
        if (s_mem) {
            return true;
        }

        FreeTexture();

        DkDevice dev = Gfx::Device();

        DkImageLayoutMaker lm;
        dkImageLayoutMakerDefaults(&lm, dev);
        lm.flags = DkImageFlags_PitchLinear;
        lm.format = DkImageFormat_RGBA8_Unorm;
        lm.dimensions[0] = VW;
        lm.dimensions[1] = VH;

        DkImageLayout lay;
        dkImageLayoutInitialize(&lay, &lm);
        s_texPitch = dkImageLayoutGetSize(&lay) / (uint32_t)VH;

        uint32_t sz = dkImageLayoutGetSize(&lay);
        uint32_t al = dkImageLayoutGetAlignment(&lay);
        sz = (sz + al - 1) & ~(al - 1);
        sz = (sz + 0xFFFu) & ~0xFFFu;

        DkMemBlockMaker mm;
        dkMemBlockMakerDefaults(&mm, dev, sz);
        mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;

        DkMemBlock mem = dkMemBlockCreate(&mm);

        if (!mem) {
            return false;
        }

        if (s_tex <= 2) {
            s_tex = Gfx::TexAlloc();

            if (s_tex < 0) {
                dkMemBlockDestroy(mem);
                return false;
            }
        }

        dkImageInitialize(&s_img, &lay, mem, 0);
        s_mem = mem;

        DkImageView view;
        dkImageViewDefaults(&view, &s_img);
        Gfx::WriteImageDesc(s_tex, &view);

        return true;
    }

    static void Cleanup()
    {
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

        if (s_vbsf) {
            av_bsf_free(&s_vbsf);
            s_vbsf = 0;
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

    static void DropHandle()
    {
        qext_album_movie_close();
    }

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

        s_size = size;
        s_avioPos = 0;
        s_idx = albumIndex;

        WAlbum::DropFull();
        WAlbum::DropThumbs();

        if (!s_pcmBuf) {
            s_pcmBuf = (int16_t *)malloc((size_t)PCM_CAP * 2u * sizeof(int16_t));
        }

        s_avioBuf = (uint8_t *)av_malloc(AVIO_CAP);

        s_frame = av_frame_alloc();
        s_aframe = av_frame_alloc();

        if (!s_sframe) {
            s_sframe = av_frame_alloc();
        }

        s_pkt = av_packet_alloc();

        if (!s_pcmBuf || !s_avioBuf || !s_frame || !s_aframe || !s_sframe || !s_pkt) {
            logging::LogLine("[video] Failed to alloc buffers");
            DropHandle();
            Cleanup();
            return false;
        }

        s_avio = avio_alloc_context(s_avioBuf, AVIO_CAP, 0, NULL, AvioRead, NULL, AvioSeek);

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

        const AVBitStreamFilter *bsf = av_bsf_get_by_name("h264_mp4toannexb");

        if (bsf) {
            if (av_bsf_alloc(bsf, &s_vbsf) == 0) {
                if (avcodec_parameters_copy(s_vbsf->par_in, s_fmt->streams[s_vstream]->codecpar) == 0) {
                    if (av_bsf_init(s_vbsf) < 0) {
                        av_bsf_free(&s_vbsf);
                        s_vbsf = 0;
                    }
                }
                else {
                    av_bsf_free(&s_vbsf);
                    s_vbsf = 0;
                }
            }
        }

        s_astream = av_find_best_stream(s_fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

        if (s_astream >= 0) {
            s_actx = OpenDecoder(s_astream);

            if (s_actx) {
                s_swr = 0;

                AVChannelLayout outL = AV_CHANNEL_LAYOUT_STEREO;
                int srrc = swr_alloc_set_opts2(&s_swr, &outL, AV_SAMPLE_FMT_S16, 48000, &s_actx->ch_layout, s_actx->sample_fmt, s_actx->sample_rate, 0, NULL);

                if (srrc == 0) {
                    srrc = swr_init(s_swr);
                }

                if (srrc != 0) {
                    logging::LogLine("[video] Failed to init audio (rc: %d)", srrc);
                    swr_free(&s_swr);
                    s_swr = 0;
                    avcodec_free_context(&s_actx);
                    s_actx = 0;
                    s_astream = -1;
                }
            }
            else {
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

        if (s_sframe->width != vp->width || s_sframe->height != vp->height) {
            av_frame_unref(s_sframe);
        }

        s_sws = sws_getContext(vp->width, vp->height, s_vhw ? AV_PIX_FMT_NV12 : (AVPixelFormat)vp->format, VW, VH, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);

        if (!s_sws) {
            logging::LogLine("[video] Failed to alloc scaler");
            DropHandle();
            Cleanup();
            return false;
        }

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

        s_fps = 30.0;

        AVStream *vst = s_fmt->streams[s_vstream];

        if (vst->avg_frame_rate.den != 0) {
            double f = av_q2d(vst->avg_frame_rate);

            if (f >= 1.0 && f <= 120.0) {
                s_fps = f;
            }
        }

        s_xferLogged = false;
        Sfx::VideoBegin();
        s_open = true;
        s_paused = false;
        s_ended = false;
        s_fresh = true;
        s_rep = 0;
        s_haveFrame = false;
        s_readEos = false;
        s_vFlushed = false;
        s_aFlushed = false;
        s_eosV = false;
        s_eosA = false;
        Step(false);

        return true;
    }

    static bool SendVideoPkt(AVPacket *pkt)
    {
        if (!s_vctx) {
            return false;
        }

        if (s_vbsf && pkt) {
            if (av_bsf_send_packet(s_vbsf, pkt) < 0) {
                return false;
            }

            for (;;) {
                AVPacket *f = av_packet_alloc();

                if (!f) {
                    break;
                }

                int rc = av_bsf_receive_packet(s_vbsf, f);

                if (rc == 0) {
                    int s = avcodec_send_packet(s_vctx, f);
                    av_packet_free(&f);

                    if (s < 0 && s != AVERROR(EAGAIN)) {
                        return false;
                    }
                }
                else {
                    av_packet_free(&f);
                    break;
                }
            }

            return true;
        }

        int rc = avcodec_send_packet(s_vctx, pkt);

        return rc == 0 || rc == AVERROR(EAGAIN);
    }

    static bool RecvVideo()
    {
        if (!s_vctx || s_haveFrame) {
            return s_haveFrame;
        }

        av_frame_unref(s_frame);

        int rc = avcodec_receive_frame(s_vctx, s_frame);

        if (rc == AVERROR(EAGAIN)) {
            return false;
        }

        if (rc == AVERROR_EOF) {
            s_eosV = true;
            return false;
        }

        if (rc < 0) {
            return false;
        }

        AVFrame *src = s_frame;

        if (src->format == AV_PIX_FMT_NVTEGRA) {
            if (av_hwframe_transfer_data(s_sframe, src, 0) < 0) {
                if (!s_xferLogged) {
                    logging::LogLine("[video] Failed to download hw frame");
                    s_xferLogged = true;
                }

                return false;
            }

            src = s_sframe;
        }

        int64_t ts = s_frame->best_effort_timestamp;

        if (ts == AV_NOPTS_VALUE) {
            ts = s_frame->pts;
        }

        s_framePts = ts * av_q2d(s_fmt->streams[s_vstream]->time_base);

        uint8_t *dst[4] = { (uint8_t *)dkMemBlockGetCpuAddr(s_mem), 0, 0, 0 };
        int stride[4] = { (int)s_texPitch, 0, 0, 0 };
        sws_scale(s_sws, src->data, src->linesize, 0, src->height, dst, stride);
        s_haveFrame = true;

        return true;
    }

    static bool RecvAudio()
    {
        if (!s_actx || !s_swr) {
            return false;
        }

        av_frame_unref(s_aframe);

        int rc = avcodec_receive_frame(s_actx, s_aframe);

        if (rc == AVERROR(EAGAIN)) {
            return false;
        }

        if (rc == AVERROR_EOF) {
            s_eosA = true;
            return false;
        }

        if (rc < 0) {
            return false;
        }

        uint8_t *out[1] = { (uint8_t *)s_pcmBuf };
        int got = swr_convert(s_swr, out, PCM_CAP, (const uint8_t **)s_aframe->extended_data, s_aframe->nb_samples);

        if (got > 0) {
            Sfx::VideoPush(s_pcmBuf, (unsigned)got);
            return true;
        }

        return false;
    }

    static void Iterate()
    {
        if (!s_open) {
            return;
        }

        bool needV = !s_haveFrame && !s_eosV && s_vctx;
        bool needA = s_actx && s_swr && !s_eosA && Sfx::VideoHungry();

        if (!needV && !needA) {
            return;
        }

        if (needV) {
            RecvVideo();
        }

        if (needA) {
            RecvAudio();
        }

        needV = !s_haveFrame && !s_eosV && s_vctx;
        needA = s_actx && s_swr && !s_eosA && Sfx::VideoHungry();

        if (!needV && !needA) {
            return;
        }

        if (s_readEos) {
            if (!s_vFlushed && s_vctx) {
                SendVideoPkt(NULL);
                s_vFlushed = true;
            }

            if (!s_aFlushed && s_actx) {
                avcodec_send_packet(s_actx, NULL);
                s_aFlushed = true;
            }

            return;
        }

        if (av_read_frame(s_fmt, s_pkt) < 0) {
            s_readEos = true;
            return;
        }

        if (s_pkt->stream_index == s_vstream) {
            SendVideoPkt(s_pkt);
        }
        else if (s_pkt->stream_index == s_astream) {
            avcodec_send_packet(s_actx, s_pkt);
        }

        av_packet_unref(s_pkt);
    }

    static void Upload()
    {
    }
    static void Step(bool force)
    {
        if (!s_open) {
            return;
        }

        if (s_paused && !force) {
            return;
        }

        if (s_ended && !force) {
            return;
        }

        int guard = 0;

        while (guard++ < 32) {
            bool needV = !s_haveFrame && !s_eosV;
            bool needA = s_actx && !s_eosA && Sfx::VideoHungry();

            if (!needV && !needA) {
                break;
            }

            Iterate();
        }

        if (force) {
            if (s_haveFrame) {
                Upload();
                s_haveFrame = false;
            }

            return;
        }

        if (s_haveFrame) {
            double clock = Sfx::VideoTime();

            if (s_framePts < clock - 0.25) {
                s_haveFrame = false;
            }
            else if (s_framePts <= clock + 0.001) {
                Upload();
                s_haveFrame = false;
            }
        }

        if (s_readEos && s_eosV && (s_eosA || !s_actx) && !s_haveFrame) {
            if (!s_ended && s_dur > 2.0 && Sfx::VideoTime() < s_dur - 2.0) {
                logging::LogLine("[video] Failed to finish (t: %.1f dur: %.1f)", Sfx::VideoTime(), s_dur);
            }

            s_ended = true;
            s_paused = true;
        }
    }

    static void SeekAbs(double t)
    {
        if (!s_open) {
            return;
        }

        if (s_dur > 0.0) {
            if (t < 0.0) {
                t = 0.0;
            }

            if (t > s_dur) {
                t = s_dur;
            }
        }
        else if (t < 0.0) {
            t = 0.0;
        }

        int64_t ts = (int64_t)(t * AV_TIME_BASE);

        if (av_seek_frame(s_fmt, -1, ts, AVSEEK_FLAG_BACKWARD) < 0) {
            return;
        }

        if (s_vctx) {
            avcodec_flush_buffers(s_vctx);
        }

        if (s_actx) {
            avcodec_flush_buffers(s_actx);
        }

        if (s_vbsf) {
            av_bsf_flush(s_vbsf);
        }

        s_readEos = false;
        s_vFlushed = false;
        s_aFlushed = false;
        s_eosV = false;
        s_eosA = false;
        s_ended = false;
        s_haveFrame = false;
        Sfx::VideoSeekTo(t);
        Step(true);
    }

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

    bool IsOpen()
    {
        return s_open;
    }

    void Close()
    {
        if (!s_open) {
            return;
        }

        s_open = false;

        if (s_vctx) {
            avcodec_send_packet(s_vctx, NULL);

            for (;;) {
                int rc = avcodec_receive_frame(s_vctx, s_frame);

                if (rc != 0) {
                    break;
                }

                av_frame_unref(s_frame);
            }

            av_frame_unref(s_frame);
        }

        if (s_actx) {
            avcodec_send_packet(s_actx, NULL);

            for (;;) {
                int rc = avcodec_receive_frame(s_actx, s_aframe);

                if (rc != 0) {
                    break;
                }

                av_frame_unref(s_aframe);
            }

            av_frame_unref(s_aframe);
        }

        Cleanup();
        DropHandle();
        Sfx::VideoEnd();
        s_idx = -1;
    }

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

        if (s_avioBuf) {
            av_free(s_avioBuf);
            s_avioBuf = 0;
        }

        if (s_sframe) {
            av_frame_free(&s_sframe);
            s_sframe = 0;
        }
    }

    void Draw()
    {
        if (!s_open) {
            return;
        }

        Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

        if (s_tex > 2) {
            Gfx::PushIcon(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, s_tex, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
        }
    }

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

        Step(false);

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
            if (s_ended) {
                SeekAbs(0.0);
                s_paused = false;
            }
            else {
                s_paused = !s_paused;
            }

            return true;
        }

        if (down & HidNpadButton_L) {
            SeekAbs(Sfx::VideoTime() - 5.0);
            return true;
        }

        if (down & HidNpadButton_R) {
            SeekAbs(Sfx::VideoTime() + 5.0);
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

    static double ProbeDuration(int albumIndex)
    {
        if (!qext_album_movie_open(albumIndex)) {
            return -1.0;
        }

        u64 size = qext_album_movie_size();

        if (size == 0) {
            qext_album_movie_close();
            return -1.0;
        }

        u64 ss = s_size;
        u64 sp = s_avioPos;
        s_size = size;
        s_avioPos = 0;

        double dur = -1.0;
        uint8_t *buf = (uint8_t *)av_malloc(AVIO_CAP);
        AVIOContext *avio = 0;

        if (buf) {
            avio = avio_alloc_context(buf, AVIO_CAP, 0, NULL, AvioRead, NULL, AvioSeek);
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
        s_size = ss;
        s_avioPos = sp;

        return dur;
    }

    double Duration(int albumIndex)
    {
        if (albumIndex < 0 || albumIndex >= 256) {
            return -1.0;
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

    void DurReset()
    {
        s_durInit = false;
    }

} // namespace WVideo
