/* NekoMenu Album submenu (C) 2026 Souldbminer */
/* Licensed under the GPLv2             */

#include "common.hpp"
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

namespace Video {

    constexpr int kVideoWidth = 1280;
    constexpr int kVideoHeight = 720;
    constexpr int kAvioCapacity = 65536;
    constexpr int kPcmCapacity = 8192;
    constexpr int kVideoQueueCapacity = 240;

    /// @brief Per-reader AVIO cursor. Probe and playback each own one, so a
    /// duration probe never clobbers the playback position/size.
    struct AvioCtx {
        u64 pos;
        u64 size;
    };

    /// @brief Canonical playback phase. Mirrors open/paused/ended.
    enum class PlaybackPhase {
        Closed,
        Playing,
        Paused,
        Ended
    };

    /// @brief All mutable video player state in one place.
    struct VideoState {
        PlaybackPhase phase = PlaybackPhase::Closed;
        bool isOpen = false;
        bool isFresh = false;
        int repeatCount = 0;
        int albumIndex = -1;
        double osdTouchedAt = -100.0; /* menu time of the last OSD touch */

        AVFormatContext *formatContext = nullptr;
        AVCodecContext *videoCodecContext = nullptr;
        AVCodecContext *audioCodecContext = nullptr;
        int videoStreamIndex = -1;
        int audioStreamIndex = -1;
        AVFrame *videoFrame = nullptr;
        AVFrame *audioFrame = nullptr;
        AVPacket *packet = nullptr;
        AVFrame *softwareFrame = nullptr;
        bool useHardwareDecode = false;
        bool transferErrorLogged = false;
        AVBufferRef *hardwareDevice = nullptr;
        SwsContext *scalerContext = nullptr;
        SwrContext *resamplerContext = nullptr;
        AVIOContext *avioContext = nullptr;
        uint8_t *avioBuffer = nullptr;

        double durationSeconds = 0.0;
        int64_t videoStartTime = 0;
        double videoTimeBase = 0.0;
        int64_t audioStartTime = 0;
        double audioTimeBase = 0.0;

        AvioCtx avioState{};
        pthread_t decodeThread{};
        bool isThreadRunning = false;
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        bool stopRequested = false;
        bool seekRequested = false;
        double seekTargetSeconds = 0.0;
        double clockBaseSeconds = 0.0;
        double pausePositionSeconds = 0.0;
        u64 clockTickNanos = 0;
        bool isPaused = false;
        bool hasEnded = false;

        AVPacket *packetQueue[kVideoQueueCapacity]{};
        int queueHead = 0;
        int queueTail = 0;
        int queueCount = 0;

        int16_t *pcmBuffer = nullptr;
        uint8_t *rgbBuffers[2]{};
        int readyBufferIndex = -1;
        int shownBufferIndex = -1;

        int textureSlot = -1;

        double durationCache[256]{};
        bool durationCacheInit = false;
    };

    static VideoState s_state;

    /// @brief Monotonic time in nanoseconds.
    static u64 NowTicks()
    {
        struct timespec timeSpec;
        clock_gettime(CLOCK_MONOTONIC, &timeSpec);
        return (u64)timeSpec.tv_sec * 1000000000ull + (u64)timeSpec.tv_nsec;
    }

    /// @brief Sleep a few milliseconds
    static void SleepMs(int delayMs)
    {
        struct timespec sleepSpec;
        sleepSpec.tv_sec = delayMs / 1000;
        sleepSpec.tv_nsec = (long)(delayMs % 1000) * 1000000L;
        nanosleep(&sleepSpec, NULL);
    }

    /// @brief Transition phase while the caller already holds the mutex.
    /// @param newPhase Target phase.
    static void SetPhaseLocked(PlaybackPhase newPhase)
    {
        s_state.phase = newPhase;
        s_state.isOpen = (newPhase != PlaybackPhase::Closed);
        s_state.isPaused = (newPhase == PlaybackPhase::Paused || newPhase == PlaybackPhase::Ended);
        s_state.hasEnded = (newPhase == PlaybackPhase::Ended);
    }

    /// @brief Transition the canonical playback phase (mirrors open/paused/ended).
    /// @param newPhase Target phase.
    static void SetPhase(PlaybackPhase newPhase)
    {
        pthread_mutex_lock(&s_state.mutex);
        SetPhaseLocked(newPhase);
        pthread_mutex_unlock(&s_state.mutex);
    }

    /// @brief Playback clock
    static double ClockNow()
    {
        pthread_mutex_lock(&s_state.mutex);
        double currentTime;

        if (s_state.isPaused) {
            currentTime = s_state.pausePositionSeconds;
        } else {
            currentTime = s_state.clockBaseSeconds + (double)(NowTicks() - s_state.clockTickNanos) / 1e9;
        }

        pthread_mutex_unlock(&s_state.mutex);
        return currentTime;
    }

    /// @brief Reset the playback clock to a position.
    /// @param positionSeconds New position in seconds.
    static void ClockSet(double positionSeconds)
    {
        pthread_mutex_lock(&s_state.mutex);
        s_state.clockBaseSeconds = s_state.pausePositionSeconds = positionSeconds;
        s_state.clockTickNanos = NowTicks();
        pthread_mutex_unlock(&s_state.mutex);
    }

    /// @brief Freeze or resume the playback clock (and the audio output).
    /// @param paused True to freeze.
    static void SetPaused(bool paused)
    {
        pthread_mutex_lock(&s_state.mutex);

        if (paused == s_state.isPaused) {
            if (s_state.hasEnded) {
                s_state.phase = PlaybackPhase::Ended;
            } else if (s_state.phase != PlaybackPhase::Closed) {
                s_state.phase = paused ? PlaybackPhase::Paused : PlaybackPhase::Playing;
            }

            pthread_mutex_unlock(&s_state.mutex);
            return;
        }

        u64 nowNanos = NowTicks();

        if (paused) {
            s_state.pausePositionSeconds = s_state.clockBaseSeconds + (double)(nowNanos - s_state.clockTickNanos) / 1e9;
            s_state.isPaused = true;

            if (s_state.hasEnded) {
                s_state.phase = PlaybackPhase::Ended;
            } else if (s_state.phase != PlaybackPhase::Closed) {
                s_state.phase = PlaybackPhase::Paused;
            }
        } else {
            s_state.clockBaseSeconds = s_state.pausePositionSeconds;
            s_state.clockTickNanos = nowNanos;
            s_state.isPaused = false;

            if (s_state.hasEnded) {
                s_state.hasEnded = false;
                s_state.phase = PlaybackPhase::Playing;
            } else if (s_state.phase != PlaybackPhase::Closed) {
                s_state.phase = PlaybackPhase::Playing;
            }
        }

        pthread_mutex_unlock(&s_state.mutex);
        g_host->sfxVideoSetPaused(paused ? 1 : 0);
    }

    /// @brief Whether the worker was asked to stop.
    /// @return True when stopping.
    static bool Stopped()
    {
        pthread_mutex_lock(&s_state.mutex);
        bool stopFlag = s_state.stopRequested;
        pthread_mutex_unlock(&s_state.mutex);
        return stopFlag;
    }

    /// @brief Whether a seek is pending (worker peek, keeps the request).
    /// @return True when a seek is pending.
    static bool SeekPending()
    {
        pthread_mutex_lock(&s_state.mutex);
        bool isPending = s_state.seekRequested;
        pthread_mutex_unlock(&s_state.mutex);
        return isPending;
    }

    /// @brief Take a pending seek request.
    /// @param targetSecondsOut Target time output.
    /// @return True when a request was pending.
    static bool TakeSeek(double *targetSecondsOut)
    {
        pthread_mutex_lock(&s_state.mutex);
        bool hasRequest = s_state.seekRequested;

        if (hasRequest) {
            *targetSecondsOut = s_state.seekTargetSeconds;
            s_state.seekRequested = false;
            s_state.hasEnded = false;

            if (s_state.phase == PlaybackPhase::Ended) {
                s_state.phase = s_state.isPaused ? PlaybackPhase::Paused : PlaybackPhase::Playing;
            }
        }

        pthread_mutex_unlock(&s_state.mutex);
        return hasRequest;
    }

    /// @brief FFmpeg read callback pulling from the album movie handle.
    /// @param opaque AvioCtx for this reader.
    /// @param outputBuffer Output buffer.
    /// @param requestSize Bytes requested.
    /// @return Bytes read, or AVERROR_EOF.
    static int AvioRead(void *opaque, uint8_t *outputBuffer, int requestSize)
    {
        AvioCtx *readerState = (AvioCtx *)opaque;

        if (!readerState || !outputBuffer || requestSize <= 0) {
            return AVERROR_EOF;
        }

        int totalRead = 0;

        while (totalRead < requestSize) {
            int bytesRead = g_host->qextAlbumMovieRead(readerState->pos, outputBuffer + totalRead, (unsigned)(requestSize - totalRead));

            if (bytesRead <= 0) {
                break;
            }

            totalRead += bytesRead;
            readerState->pos += (u64)bytesRead;
        }

        if (totalRead == 0) {
            return AVERROR_EOF;
        }

        return totalRead;
    }

    /// @brief FFmpeg seek callback over the album movie handle.
    /// @param opaque AvioCtx for this reader.
    /// @param offset Byte offset.
    /// @param whence Seek mode (or AVSEEK_SIZE for size query).
    /// @return New position, or the size for AVSEEK_SIZE.
    static int64_t AvioSeek(void *opaque, int64_t offset, int whence)
    {
        AvioCtx *readerState = (AvioCtx *)opaque;

        if (!readerState) {
            return AVERROR_EOF;
        }

        if (whence == AVSEEK_SIZE) {
            return (int64_t)readerState->size;
        }

        if (offset < 0) {
            offset = 0;
        }

        if ((u64)offset > readerState->size) {
            offset = (int64_t)readerState->size;
        }

        readerState->pos = (u64)offset;

        return offset;
    }

    /// @brief Free the video frame texture.
    static void FreeTexture()
    {
        g_host->gfxTexFree(s_state.textureSlot);
        s_state.textureSlot = -1;
    }

    /// @brief Allocate the fixed video frame texture.
    /// @return True on success.
    static bool AllocTexture()
    {
        void *lockedPixels = nullptr;
        int lockedPitch = 0;

        if (s_state.textureSlot >= 0) {
            return true;
        }

        FreeTexture();
        s_state.textureSlot = g_host->gfxTexAlloc();

        if (s_state.textureSlot < 0) {
            return false;
        }

        if (g_host->gfxTexLock(s_state.textureSlot, kVideoWidth, kVideoHeight, &lockedPixels, &lockedPitch) == 0 || !lockedPixels || lockedPitch <= 0) {
            FreeTexture();
            return false;
        }

        g_host->gfxTexUnlock(s_state.textureSlot);
        return true;
    }

    /// @brief Free all FFmpeg contexts, the packet queue and IO buffers.
    static void Cleanup()
    {
        for (int queueIndex = 0; queueIndex < kVideoQueueCapacity; queueIndex++) {
            if (s_state.packetQueue[queueIndex]) {
                av_packet_free(&s_state.packetQueue[queueIndex]);
                s_state.packetQueue[queueIndex] = 0;
            }
        }

        s_state.queueHead = s_state.queueTail = s_state.queueCount = 0;

        if (s_state.resamplerContext) {
            swr_free(&s_state.resamplerContext);
            s_state.resamplerContext = 0;
        }

        if (s_state.scalerContext) {
            sws_freeContext(s_state.scalerContext);
            s_state.scalerContext = 0;
        }

        if (s_state.packet) {
            av_packet_free(&s_state.packet);
            s_state.packet = 0;
        }

        if (s_state.videoFrame) {
            av_frame_free(&s_state.videoFrame);
            s_state.videoFrame = 0;
        }

        if (s_state.audioFrame) {
            av_frame_free(&s_state.audioFrame);
            s_state.audioFrame = 0;
        }

        if (s_state.videoCodecContext) {
            avcodec_free_context(&s_state.videoCodecContext);
            s_state.videoCodecContext = 0;
        }

        if (s_state.audioCodecContext) {
            avcodec_free_context(&s_state.audioCodecContext);
            s_state.audioCodecContext = 0;
        }

        if (s_state.formatContext) {
            avformat_close_input(&s_state.formatContext);
            s_state.formatContext = 0;
        }

        if (s_state.avioContext) {
            av_free(s_state.avioContext->buffer);
            avio_context_free(&s_state.avioContext);
            s_state.avioBuffer = 0;
        }

        /* Release the NVTEGRA device so its channels close and its mmu clock
         * requests finalize; the engines must fully idle between videos.
         * Recreated on next open by EnsureHwDev. Decoders are already freed
         * above, so no session still references it. */
        av_buffer_unref(&s_state.hardwareDevice);

        s_state.videoStreamIndex = -1;
        s_state.audioStreamIndex = -1;
        s_state.useHardwareDecode = false;
    }

    /// @brief Close the album movie handle without touching decoders.
    static void DropHandle()
    {
        g_host->qextAlbumMovieClose();
    }

    /// @brief Ensure the NVTEGRA hardware device exists.
    /// @return True when ready.
    static bool EnsureHwDev()
    {
        if (s_state.hardwareDevice) {
            return true;
        }

        if (av_hwdevice_ctx_create(&s_state.hardwareDevice, AV_HWDEVICE_TYPE_NVTEGRA, NULL, NULL, 0) != 0) {
            s_state.hardwareDevice = 0;
            LogFmt("[video] Failed to init hw device");
            return false;
        }

        return true;
    }

    /// @brief Pick an HW-friendly pixel format for the decoder.
    /// @param codecContext Codec context (unused).
    /// @param candidates Candidate formats.
    /// @return Chosen format.
    static enum AVPixelFormat HwGetFormat(AVCodecContext *codecContext, const enum AVPixelFormat *candidates)
    {
        (void)codecContext;

        for (const enum AVPixelFormat *candidate = candidates; *candidate != AV_PIX_FMT_NONE; candidate++) {
            if (*candidate == AV_PIX_FMT_NVTEGRA) {
                return AV_PIX_FMT_NVTEGRA;
            }
        }

        return candidates[0];
    }

    /// @brief Open a decoder for one stream, with HW accel for H264.
    /// @param streamIndex Stream index in the format context.
    /// @return Codec context, or null on failure.
    static AVCodecContext *OpenDecoder(int streamIndex)
    {
        if (!s_state.formatContext || streamIndex < 0) {
            return 0;
        }

        AVStream *stream = s_state.formatContext->streams[streamIndex];
        const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);

        if (!decoder) {
            LogFmt("[video] Failed to find decoder (id: %d)", (int)stream->codecpar->codec_id);
            return 0;
        }

        AVCodecContext *codecContext = avcodec_alloc_context3(decoder);

        if (!codecContext) {
            LogFmt("[video] Failed to alloc decoder (id: %d)", (int)stream->codecpar->codec_id);
            return 0;
        }

        int paramResult = avcodec_parameters_to_context(codecContext, stream->codecpar);

        if (paramResult < 0) {
            LogFmt("[video] Failed to setup decoder (rc: %d)", paramResult);
            avcodec_free_context(&codecContext);
            return 0;
        }

        codecContext->thread_count = 1;

        if (stream->codecpar->codec_id == AV_CODEC_ID_H264) {
            codecContext->sw_pix_fmt = AV_PIX_FMT_NV12;

            if (EnsureHwDev()) {
                codecContext->hw_device_ctx = av_buffer_ref(s_state.hardwareDevice);

                if (codecContext->hw_device_ctx) {
                    codecContext->get_format = HwGetFormat;
                }
            }
        }

        int openResult = avcodec_open2(codecContext, decoder, NULL);

        if (openResult < 0) {
            LogFmt("[video] Failed to open decoder (rc: %d)", openResult);
            avcodec_free_context(&codecContext);
            return 0;
        }

        if (stream->codecpar->codec_id == AV_CODEC_ID_H264 && codecContext->hw_device_ctx) {
            s_state.useHardwareDecode = true;
        }

        return codecContext;
    }

    /// @brief Build sampler for audio decoder
    /// @return True when ready.
    static bool SetupResampler()
    {
        if (s_state.resamplerContext) {
            swr_free(&s_state.resamplerContext);
            s_state.resamplerContext = 0;
        }

        if (!s_state.audioCodecContext) {
            return false;
        }

        AVChannelLayout stereoLayout = AV_CHANNEL_LAYOUT_STEREO;
        SwrContext *resampler = 0;
        int setupResult = swr_alloc_set_opts2(&resampler, &stereoLayout, AV_SAMPLE_FMT_S16, 48000,
                                     &s_state.audioCodecContext->ch_layout, s_state.audioCodecContext->sample_fmt,
                                     s_state.audioCodecContext->sample_rate, 0, NULL);

        if (setupResult == 0) {
            setupResult = swr_init(resampler);
        }

        if (setupResult != 0) {
            swr_free(&resampler);
            return false;
        }

        s_state.resamplerContext = resampler;
        return true;
    }

    /// @brief Worker side of a seek: land on the keyframe at or before the
    /// target, then decode forward silently so the first picture and sound
    /// are exactly at it (sLaunch DoSeek).
    /// @param targetSeconds Target time in seconds.
    static void DoSeek(double targetSeconds)
    {
        if (!s_state.formatContext || s_state.videoStreamIndex < 0) {
            return;
        }

        if (s_state.durationSeconds > 0.0) {
            if (targetSeconds < 0.0) {
                targetSeconds = 0.0;
            }

            if (targetSeconds > s_state.durationSeconds - 0.1) {
                targetSeconds = s_state.durationSeconds - 0.1;
            }

            if (targetSeconds < 0.0) {
                targetSeconds = 0.0;
            }
        } else if (targetSeconds < 0.0) {
            targetSeconds = 0.0;
        }

        AVStream *videoStream = s_state.formatContext->streams[s_state.videoStreamIndex];
        int64_t seekTimestamp = s_state.videoStartTime + (int64_t)(targetSeconds / (s_state.videoTimeBase > 0.0 ? s_state.videoTimeBase : 1.0 / 30.0));
        av_seek_frame(s_state.formatContext, s_state.videoStreamIndex, seekTimestamp, AVSEEK_FLAG_BACKWARD);

        if (s_state.videoCodecContext) {
            avcodec_flush_buffers(s_state.videoCodecContext);
        }

        if (s_state.audioCodecContext) {
            avcodec_flush_buffers(s_state.audioCodecContext);
            SetupResampler();
        }

        for (int queueIndex = 0; queueIndex < kVideoQueueCapacity; queueIndex++) {
            if (s_state.packetQueue[queueIndex]) {
                av_packet_free(&s_state.packetQueue[queueIndex]);
                s_state.packetQueue[queueIndex] = 0;
            }
        }

        s_state.queueHead = s_state.queueTail = s_state.queueCount = 0;

        if (s_state.videoFrame) {
            av_frame_unref(s_state.videoFrame);
        }

        if (s_state.audioFrame) {
            av_frame_unref(s_state.audioFrame);
        }

        if (s_state.softwareFrame) {
            av_frame_unref(s_state.softwareFrame);
        }

        g_host->sfxVideoSeekTo(targetSeconds);
        ClockSet(targetSeconds);

        pthread_mutex_lock(&s_state.mutex);
        s_state.hasEnded = false;

        if (s_state.phase == PlaybackPhase::Ended) {
            s_state.phase = s_state.isPaused ? PlaybackPhase::Paused : PlaybackPhase::Playing;
        }

        pthread_mutex_unlock(&s_state.mutex);
    }

    /// @brief Decode one demuxed audio packet into the SFX ring.
    /// @param audioPacket Demuxed audio packet (borrowed).
    /// @param skipUntilSeconds Silence before this time.
    static void DecodeAudio(AVPacket *audioPacket, double skipUntilSeconds)
    {
        if (!s_state.audioCodecContext || !s_state.resamplerContext || !audioPacket) {
            return;
        }

        if (avcodec_send_packet(s_state.audioCodecContext, audioPacket) < 0) {
            return;
        }

        while (!Stopped() && !SeekPending() && avcodec_receive_frame(s_state.audioCodecContext, s_state.audioFrame) == 0) {
            int sampleCount = s_state.audioFrame->nb_samples;
            int sampleRate = s_state.audioFrame->sample_rate > 0 ? s_state.audioFrame->sample_rate : 48000;
            int64_t frameTimestamp = s_state.audioFrame->best_effort_timestamp;

            double presentationTime = skipUntilSeconds;

            if (frameTimestamp != AV_NOPTS_VALUE && s_state.audioTimeBase > 0.0) {
                presentationTime = (double)(frameTimestamp - s_state.audioStartTime) * s_state.audioTimeBase;
            }

            if (sampleCount <= 0 || presentationTime + (double)sampleCount / (double)sampleRate < skipUntilSeconds) {
                av_frame_unref(s_state.audioFrame);
                continue;
            }

            uint8_t *outputPlanes[1] = { (uint8_t *)s_state.pcmBuffer };
            int convertedSamples = swr_convert(s_state.resamplerContext, outputPlanes, kPcmCapacity,
                                  (const uint8_t **)s_state.audioFrame->extended_data, sampleCount);
            av_frame_unref(s_state.audioFrame);

            if (convertedSamples > 0) {
                g_host->sfxVideoPush(s_state.pcmBuffer, (unsigned)convertedSamples);
            }
        }

        av_frame_unref(s_state.audioFrame);
    }

    /// @brief Decode worker
    static void DecodeLoop()
    {
        double skipUntilSeconds = 0.0;
        bool readAtEof = false;
        bool decoderAtEof = false;
        bool drainSent = false;
        bool haveFrame = false;
        double framePresentationTime = 0.0;
        int writeBufferIndex = 0;

        while (!Stopped()) {
            double seekTarget = 0.0;

            if (TakeSeek(&seekTarget)) {
                readAtEof = decoderAtEof = drainSent = false;
                haveFrame = false;
                DoSeek(seekTarget);
                skipUntilSeconds = seekTarget;
            }

            /* Read ahead */
            while (!readAtEof && !Stopped() && !SeekPending() && s_state.queueCount < kVideoQueueCapacity &&
                   (g_host->sfxVideoBuffered() < 0.6 || (s_state.queueCount < 6 && !haveFrame))) {
                if (av_read_frame(s_state.formatContext, s_state.packet) < 0) {
                    readAtEof = true;
                    break;
                }

                if (s_state.packet->stream_index == s_state.audioStreamIndex && s_state.audioCodecContext && s_state.resamplerContext) {
                    DecodeAudio(s_state.packet, skipUntilSeconds);
                    av_packet_unref(s_state.packet);
                } else if (s_state.packet->stream_index == s_state.videoStreamIndex) {
                    AVPacket *queuedPacket = av_packet_alloc();

                    if (queuedPacket) {
                        av_packet_move_ref(queuedPacket, s_state.packet);
                        s_state.packetQueue[s_state.queueTail] = queuedPacket;
                        s_state.queueTail = (s_state.queueTail + 1) % kVideoQueueCapacity;
                        s_state.queueCount++;
                    }

                    av_packet_unref(s_state.packet);
                } else {
                    av_packet_unref(s_state.packet);
                }
            }

            /* Next picture. */
            while (!haveFrame && !decoderAtEof && !Stopped() && !SeekPending()) {
                int receiveResult = avcodec_receive_frame(s_state.videoCodecContext, s_state.videoFrame);

                if (receiveResult == 0) {
                    int64_t frameTimestamp = s_state.videoFrame->best_effort_timestamp;

                    if (frameTimestamp == AV_NOPTS_VALUE) {
                        frameTimestamp = s_state.videoFrame->pts;
                    }

                    framePresentationTime = (frameTimestamp != AV_NOPTS_VALUE && s_state.videoTimeBase > 0.0)
                        ? (double)(frameTimestamp - s_state.videoStartTime) * s_state.videoTimeBase
                        : ClockNow();

                    if (framePresentationTime < skipUntilSeconds - 0.001) {
                        av_frame_unref(s_state.videoFrame);
                        continue;
                    }

                    haveFrame = true;
                } else if (receiveResult == AVERROR_EOF) {
                    decoderAtEof = true;
                } else if (s_state.queueCount > 0) {
                    AVPacket *queuedPacket = s_state.packetQueue[s_state.queueHead];
                    s_state.packetQueue[s_state.queueHead] = 0;
                    s_state.queueHead = (s_state.queueHead + 1) % kVideoQueueCapacity;
                    s_state.queueCount--;
                    avcodec_send_packet(s_state.videoCodecContext, queuedPacket);
                    av_packet_free(&queuedPacket);
                } else if (readAtEof && !drainSent) {
                    avcodec_send_packet(s_state.videoCodecContext, NULL);
                    drainSent = true;
                } else {
                    break;
                }
            }

            if (haveFrame) {
                if (ClockNow() < framePresentationTime) {
                    SleepMs(2);
                    continue;
                }

                AVFrame *sourceFrame = s_state.videoFrame;

                if (s_state.videoFrame->format == AV_PIX_FMT_NVTEGRA) {
                    if (av_hwframe_transfer_data(s_state.softwareFrame, s_state.videoFrame, 0) == 0) {
                        sourceFrame = s_state.softwareFrame;
                    } else {
                        if (!s_state.transferErrorLogged) {
                            LogFmt("[video] Failed to download hw frame");
                            s_state.transferErrorLogged = true;
                        }

                        av_frame_unref(s_state.videoFrame);
                        av_frame_unref(s_state.softwareFrame);
                        haveFrame = false;
                        continue;
                    }
                }

                s_state.scalerContext = sws_getCachedContext(s_state.scalerContext, sourceFrame->width, sourceFrame->height,
                                             (AVPixelFormat)sourceFrame->format,
                                             kVideoWidth, kVideoHeight, AV_PIX_FMT_RGBA,
                                             SWS_BILINEAR, NULL, NULL, NULL);

                if (!s_state.scalerContext || !s_state.rgbBuffers[writeBufferIndex]) {
                    if (!s_state.scalerContext) {
                        LogFmt("[video] Failed to alloc scaler");
                    }

                    av_frame_unref(s_state.videoFrame);
                    av_frame_unref(s_state.softwareFrame);
                    haveFrame = false;
                    continue;
                }

                uint8_t *destPlanes[4] = { s_state.rgbBuffers[writeBufferIndex], 0, 0, 0 };
                int destStrides[4] = { kVideoWidth * 4, 0, 0, 0 };
                sws_scale(s_state.scalerContext, sourceFrame->data, sourceFrame->linesize, 0, sourceFrame->height, destPlanes, destStrides);
                av_frame_unref(s_state.videoFrame);
                av_frame_unref(s_state.softwareFrame);
                haveFrame = false;

                __atomic_store_n(&s_state.readyBufferIndex, writeBufferIndex, __ATOMIC_RELEASE);
                writeBufferIndex ^= 1;
                continue;
            }

            if (readAtEof && s_state.queueCount == 0 && (decoderAtEof || drainSent)) {
                if (ClockNow() >= s_state.durationSeconds - 0.05) {
                    SetPaused(true);
                    SetPhase(PlaybackPhase::Ended);
                }
            }

            SleepMs(5);
        }

        for (int queueIndex = 0; queueIndex < s_state.queueCount; queueIndex++) {
            int queueSlot = (s_state.queueHead + queueIndex) % kVideoQueueCapacity;
            av_packet_free(&s_state.packetQueue[queueSlot]);
            s_state.packetQueue[queueSlot] = 0;
        }

        s_state.queueHead = s_state.queueTail = s_state.queueCount = 0;

        if (haveFrame) {
            av_frame_unref(s_state.videoFrame);
        }
    }

    /// @brief Worker thread entry.
    /// @param threadArg Unused.
    /// @return Null.
    static void *DecodeEntry(void *threadArg)
    {
        (void)threadArg;
        DecodeLoop();
        return NULL;
    }

    /// @brief Open a movie entry for playback.
    /// @param albumIndex Album entry index.
    /// @return True on success.
    bool Open(int albumIndex)
    {
        Close();

        if (albumIndex < 0 || g_host->qextAlbumIsMovie(albumIndex) <= 0) {
            LogFmt("[video] Failed to open video (idx: %d)", albumIndex);
            return false;
        }

        if (g_host->qextAlbumMovieOpen(albumIndex) == 0) {
            LogFmt("[video] Failed to open video (idx: %d)", albumIndex);
            return false;
        }

        u64 movieSize = (u64)g_host->qextAlbumMovieSize();

        if (movieSize == 0) {
            LogFmt("[video] Failed to get size (idx: %d)", albumIndex);
            g_host->qextAlbumMovieClose();
            return false;
        }

        s_state.avioState.pos = 0;
        s_state.avioState.size = movieSize;
        s_state.albumIndex = albumIndex;

        Album::DropFull();
        Album::DropThumbs();

        if (!s_state.pcmBuffer) {
            s_state.pcmBuffer = (int16_t *)malloc((size_t)kPcmCapacity * 2u * sizeof(int16_t));
        }

        for (int bufferIndex = 0; bufferIndex < 2; bufferIndex++) {
            if (!s_state.rgbBuffers[bufferIndex]) {
                s_state.rgbBuffers[bufferIndex] = (uint8_t *)malloc((size_t)kVideoWidth * (size_t)kVideoHeight * 4u);
            }
        }

        s_state.avioBuffer = (uint8_t *)av_malloc(kAvioCapacity);

        s_state.videoFrame = av_frame_alloc();
        s_state.audioFrame = av_frame_alloc();

        if (!s_state.softwareFrame) {
            s_state.softwareFrame = av_frame_alloc();
        }

        s_state.packet = av_packet_alloc();

        if (!s_state.pcmBuffer || !s_state.rgbBuffers[0] || !s_state.rgbBuffers[1] || !s_state.avioBuffer ||
            !s_state.videoFrame || !s_state.audioFrame || !s_state.softwareFrame || !s_state.packet) {
            LogFmt("[video] Failed to alloc buffers");
            DropHandle();
            Cleanup();
            return false;
        }

        s_state.avioContext = avio_alloc_context(s_state.avioBuffer, kAvioCapacity, 0, &s_state.avioState, AvioRead, NULL, AvioSeek);

        if (!s_state.avioContext) {
            LogFmt("[video] Failed to alloc avio");
            DropHandle();
            Cleanup();
            return false;
        }

        s_state.formatContext = avformat_alloc_context();

        if (!s_state.formatContext) {
            LogFmt("[video] Failed to alloc format");
            DropHandle();
            Cleanup();
            return false;
        }

        s_state.formatContext->pb = s_state.avioContext;
        s_state.formatContext->probesize = 256 * 1024;
        s_state.formatContext->max_analyze_duration = 0;

        int openResult = avformat_open_input(&s_state.formatContext, NULL, NULL, NULL);

        if (openResult < 0) {
            LogFmt("[video] Failed to open input (rc: %d)", openResult);
            avformat_close_input(&s_state.formatContext);
            DropHandle();
            Cleanup();
            return false;
        }

        int streamInfoResult = avformat_find_stream_info(s_state.formatContext, NULL);

        if (streamInfoResult < 0) {
            LogFmt("[video] Failed to read info (rc: %d)", streamInfoResult);
            DropHandle();
            Cleanup();
            return false;
        }

        s_state.videoStreamIndex = av_find_best_stream(s_state.formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

        if (s_state.videoStreamIndex < 0) {
            LogFmt("[video] Failed to find video");
            DropHandle();
            Cleanup();
            return false;
        }

        s_state.videoCodecContext = OpenDecoder(s_state.videoStreamIndex);

        if (!s_state.videoCodecContext) {
            LogFmt("[video] Failed to open video decoder");
            DropHandle();
            Cleanup();
            return false;
        }

        s_state.audioStreamIndex = av_find_best_stream(s_state.formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

        if (s_state.audioStreamIndex >= 0) {
            s_state.audioCodecContext = OpenDecoder(s_state.audioStreamIndex);

            if (s_state.audioCodecContext) {
                if (!SetupResampler()) {
                    LogFmt("[video] Failed to init audio");
                    swr_free(&s_state.resamplerContext);
                    s_state.resamplerContext = 0;
                    avcodec_free_context(&s_state.audioCodecContext);
                    s_state.audioCodecContext = 0;
                    s_state.audioStreamIndex = -1;
                }
            } else {
                s_state.audioStreamIndex = -1;
            }
        }

        AVCodecParameters *videoParams = s_state.formatContext->streams[s_state.videoStreamIndex]->codecpar;

        if (videoParams->width <= 0 || videoParams->height <= 0) {
            LogFmt("[video] Failed to read dims (w: %d h: %d)", videoParams->width, videoParams->height);
            DropHandle();
            Cleanup();
            return false;
        }

        s_state.softwareFrame->format = AV_PIX_FMT_NV12;
        s_state.softwareFrame->width = videoParams->width;
        s_state.softwareFrame->height = videoParams->height;

        if (!AllocTexture()) {
            LogFmt("[video] Failed to alloc texture");
            DropHandle();
            Cleanup();
            return false;
        }

        s_state.durationSeconds = 0.0;

        if (s_state.formatContext->duration != AV_NOPTS_VALUE) {
            s_state.durationSeconds = s_state.formatContext->duration / (double)AV_TIME_BASE;
        }

        AVStream *videoStream = s_state.formatContext->streams[s_state.videoStreamIndex];
        s_state.videoStartTime = (videoStream->start_time != AV_NOPTS_VALUE) ? videoStream->start_time : 0;
        s_state.videoTimeBase = av_q2d(videoStream->time_base);

        if (s_state.audioStreamIndex >= 0) {
            AVStream *audioStream = s_state.formatContext->streams[s_state.audioStreamIndex];
            s_state.audioStartTime = (audioStream->start_time != AV_NOPTS_VALUE) ? audioStream->start_time : 0;
            s_state.audioTimeBase = av_q2d(audioStream->time_base);
        } else {
            s_state.audioStartTime = 0;
            s_state.audioTimeBase = 0.0;
        }

        s_state.transferErrorLogged = false;
        g_host->sfxVideoBegin();

        pthread_mutex_lock(&s_state.mutex);
        s_state.stopRequested = false;
        s_state.seekRequested = false;
        s_state.seekTargetSeconds = 0.0;
        s_state.hasEnded = false;
        pthread_mutex_unlock(&s_state.mutex);

        s_state.readyBufferIndex = -1;
        s_state.shownBufferIndex = -1;
        ClockSet(0.0);
        SetPaused(false);

        if (pthread_create(&s_state.decodeThread, NULL, DecodeEntry, NULL) != 0) {
            LogFmt("[video] Failed to start worker");
            DropHandle();
            Cleanup();
            g_host->sfxVideoEnd();
            s_state.albumIndex = -1;
            return false;
        }

        s_state.isThreadRunning = true;
        SetPhase(PlaybackPhase::Playing);
        s_state.isFresh = true;
        s_state.repeatCount = 0;
        s_state.osdTouchedAt = (double)g_host->appTime();

        return true;
    }

    /// @brief Seek to an absolute timestamp (handled by the worker).
    /// @param targetSeconds Target time in seconds.
    static void SeekAbs(double targetSeconds)
    {
        if (s_state.phase == PlaybackPhase::Closed) {
            return;
        }

        if (s_state.durationSeconds > 0.0) {
            if (targetSeconds < 0.0) {
                targetSeconds = 0.0;
            }

            if (targetSeconds > s_state.durationSeconds - 0.1) {
                targetSeconds = s_state.durationSeconds - 0.1;
            }

            if (targetSeconds < 0.0) {
                targetSeconds = 0.0;
            }
        } else if (targetSeconds < 0.0) {
            targetSeconds = 0.0;
        }

        pthread_mutex_lock(&s_state.mutex);
        s_state.seekTargetSeconds = targetSeconds;
        s_state.seekRequested = true;
        s_state.hasEnded = false;

        if (s_state.phase == PlaybackPhase::Ended) {
            s_state.phase = s_state.isPaused ? PlaybackPhase::Paused : PlaybackPhase::Playing;
        }

        pthread_mutex_unlock(&s_state.mutex);
    }

    /// @brief Step to a neighboring album entry, keeping video or image mode.
    /// @param delta Signed step.
    /// @return True when the step was handled.
    static bool StepContent(int delta)
    {
        int albumCount = g_host->qextAlbumCount();
        int targetIndex = s_state.albumIndex + delta;

        if (targetIndex < 0 || targetIndex >= albumCount) {
            return false;
        }

        if (g_host->qextAlbumIsMovie(targetIndex) <= 0) {
            Close();
            Album::OpenFull(targetIndex);
            return true;
        }

        int currentIndex = s_state.albumIndex;

        if (Open(targetIndex)) {
            return true;
        }

        Open(currentIndex);

        return true;
    }

    /// @brief Whether a video is currently open.
    /// @return True when open.
    bool IsOpen()
    {
        return s_state.phase != PlaybackPhase::Closed;
    }

    /// @brief Whether playback is paused.
    /// @return True when paused.
    static bool PausedState()
    {
        pthread_mutex_lock(&s_state.mutex);
        bool result = (s_state.phase == PlaybackPhase::Paused || s_state.phase == PlaybackPhase::Ended);
        pthread_mutex_unlock(&s_state.mutex);
        return result;
    }

    /// @brief Whether a non-looping clip reached its end.
    /// @return True when ended.
    static bool EndedState()
    {
        pthread_mutex_lock(&s_state.mutex);
        bool result = (s_state.phase == PlaybackPhase::Ended);
        pthread_mutex_unlock(&s_state.mutex);
        return result;
    }

    /// @brief Give the playback position in seconds.
    /// @return Seconds from the start, clamped to the duration.
    static double PositionState()
    {
        if (s_state.phase == PlaybackPhase::Closed) {
            return 0.0;
        }

        double positionSeconds = ClockNow();

        if (positionSeconds < 0.0) {
            positionSeconds = 0.0;
        }

        if (s_state.durationSeconds > 0.0 && positionSeconds > s_state.durationSeconds) {
            positionSeconds = s_state.durationSeconds;
        }

        return positionSeconds;
    }

    /// @brief Close the video player.
    void Close()
    {
        if (s_state.phase == PlaybackPhase::Closed && !s_state.isOpen) {
            return;
        }

        SetPhase(PlaybackPhase::Closed);

        pthread_mutex_lock(&s_state.mutex);
        s_state.stopRequested = true;
        pthread_mutex_unlock(&s_state.mutex);

        if (s_state.isThreadRunning) {
            pthread_join(s_state.decodeThread, NULL);
            s_state.isThreadRunning = false;
        }

        if (s_state.videoCodecContext) {
            av_frame_unref(s_state.videoFrame);
        }

        if (s_state.audioCodecContext) {
            av_frame_unref(s_state.audioFrame);
        }

        Cleanup();
        DropHandle();
        g_host->sfxVideoEnd();
        s_state.albumIndex = -1;

        SetPhase(PlaybackPhase::Closed);
        s_state.readyBufferIndex = -1;
        s_state.shownBufferIndex = -1;
    }

    /// @brief Drop pinned video textures.
    void DropPinned()
    {
        if (s_state.isOpen || s_state.phase != PlaybackPhase::Closed) {
            Close();
        }

        FreeTexture();

        if (s_state.pcmBuffer) {
            free(s_state.pcmBuffer);
            s_state.pcmBuffer = 0;
        }

        for (int bufferIndex = 0; bufferIndex < 2; bufferIndex++) {
            if (s_state.rgbBuffers[bufferIndex]) {
                free(s_state.rgbBuffers[bufferIndex]);
                s_state.rgbBuffers[bufferIndex] = 0;
            }
        }

        if (s_state.avioBuffer) {
            av_free(s_state.avioBuffer);
            s_state.avioBuffer = 0;
        }

        if (s_state.softwareFrame) {
            av_frame_free(&s_state.softwareFrame);
            s_state.softwareFrame = 0;
        }
    }

    /// @brief Put new frames on GPU. Call every frame.
    void Tick()
    {
        if (s_state.phase == PlaybackPhase::Closed || s_state.textureSlot < 0) {
            return;
        }

        int readyIndex = __atomic_load_n(&s_state.readyBufferIndex, __ATOMIC_ACQUIRE);

        if (readyIndex < 0 || readyIndex == s_state.shownBufferIndex || !s_state.rgbBuffers[readyIndex]) {
            return;
        }

        void *lockedPixels = nullptr;
        int lockedPitch = 0;

        if (g_host->gfxTexLock(s_state.textureSlot, kVideoWidth, kVideoHeight, &lockedPixels, &lockedPitch) == 0 || !lockedPixels || lockedPitch <= 0) {
            return;
        }

        const uint8_t *sourcePixels = s_state.rgbBuffers[readyIndex];
        uint8_t *destPixels = (uint8_t *)lockedPixels;

        for (int row = 0; row < kVideoHeight; row++) {
            memcpy(destPixels + (size_t)row * (size_t)lockedPitch, sourcePixels + (size_t)row * (size_t)kVideoWidth * 4u,
                   (size_t)kVideoWidth * 4u);
        }

        g_host->gfxTexUnlock(s_state.textureSlot);
        s_state.shownBufferIndex = readyIndex;
    }

    /// @brief Draw one hint
    /// @param labelText Hint text.
    /// @param posX left X.
    /// @param opacity Opacity 0-1.
    static void DrawOsdPill(const char *labelText, float posX, float opacity)
    {
        float pillWidth = g_host->fontMeasure(labelText, 30.0f) + 48.0f;
        g_host->gfxPushOverlayPanel(posX, 976.0f, pillWidth, 64.0f, 32.0f, 0.0f, 0.0f, 0.0f, 0.6f * opacity);
        g_host->fontDraw(labelText, posX + 24.0f, 976.0f + 44.0f, 30.0f, 1.0f, 1.0f, 1.0f, opacity);
    }

    /// @brief Draw the playback hints
    static void DrawOsd()
    {
        float idleSeconds = g_host->appTime() - (float)s_state.osdTouchedAt;
        float opacity = idleSeconds < 3.0f ? 1.0f : 1.0f - (idleSeconds - 3.0f) / 0.5f;

        if (opacity <= 0.0f) {
            return;
        }

        if (opacity > 1.0f) {
            opacity = 1.0f;
        }

        DrawOsdPill("L Previous", 60.0f, opacity);

        char middleLabel[64];
        snprintf(middleLabel, sizeof(middleLabel), "ZL -5s    A %s    ZR +5s", PausedState() ? "Play" : "Pause");
        middleLabel[sizeof(middleLabel) - 1] = 0;
        float middleWidth = g_host->fontMeasure(middleLabel, 30.0f) + 48.0f;
        DrawOsdPill(middleLabel, 960.0f - middleWidth * 0.5f, opacity);

        float rightWidth = g_host->fontMeasure("R Next", 30.0f) + 48.0f;
        DrawOsdPill("R Next", 1860.0f - rightWidth, opacity);
    }

    /// @brief Draw the current video frame and OSD.
    void Draw()
    {
        if (s_state.phase == PlaybackPhase::Closed) {
            return;
        }

        g_host->gfxPushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

        if (s_state.textureSlot >= 0) {
            g_host->gfxPushIcon(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, s_state.textureSlot, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f);
        }

        DrawOsd();
    }

    /// @brief Handle video playback input.
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    /// @return True when the input was consumed.
    bool Input(unsigned long long buttonsDown, unsigned long long buttonsHeld)
    {
        (void)buttonsHeld;

        if (s_state.phase == PlaybackPhase::Closed) {
            return false;
        }

        if (buttonsDown) {
            s_state.osdTouchedAt = (double)g_host->appTime();
        }

        if (s_state.isFresh) {
            s_state.isFresh = false;
            return true;
        }

        u64 seekButtons = HidNpadButton_L | HidNpadButton_R;
        u64 provenDown = (u64)buttonsDown;
        u64 provenHeld = (u64)buttonsHeld;

        if (provenDown & seekButtons) {
            s_state.repeatCount = 0;
        }
        else if (provenHeld & seekButtons) {
            s_state.repeatCount++;

            if (s_state.repeatCount > 20 && (s_state.repeatCount % 6) == 0) {
                provenDown |= provenHeld & seekButtons;
            }
        }
        else {
            s_state.repeatCount = 0;
        }

        if (provenDown & HidNpadButton_B) {
            Close();
            g_host->sfxPlayByName("back");
            return true;
        }

        if (provenDown & HidNpadButton_A) {
            if (EndedState()) {
                SeekAbs(0.0);
                SetPaused(false);
            }
            else {
                SetPaused(!PausedState());
            }

            return true;
        }

        if (provenDown & HidNpadButton_L) {
            StepContent(-1);
            return true;
        }

        if (provenDown & HidNpadButton_R) {
            StepContent(1);
            return true;
        }

        if (provenDown & HidNpadButton_ZL) {
            SeekAbs(PositionState() - 5.0);
            return true;
        }

        if (provenDown & HidNpadButton_ZR) {
            SeekAbs(PositionState() + 5.0);
            return true;
        }

        if (provenDown & (HidNpadButton_AnyLeft | HidNpadButton_AnyRight)) {
            if (provenDown & HidNpadButton_AnyLeft) {
                StepContent(-1);
            }
            else {
                StepContent(1);
            }

            return true;
        }

        return true;
    }

    /// @brief get a movie's duration without disturbing playback.
    /// @param albumIndex Album entry index.
    /// @return Duration in seconds, or negative on failure.
    static double GetDuration(int albumIndex)
    {
        bool wasAlreadyOpen = (s_state.phase != PlaybackPhase::Closed);
        int keepAlbumIndex = s_state.albumIndex;

        if (g_host->qextAlbumMovieOpen(albumIndex) == 0) {
            return -1.0;
        }

        u64 movieSize = (u64)g_host->qextAlbumMovieSize();

        if (movieSize == 0) {
            g_host->qextAlbumMovieClose();

            if (wasAlreadyOpen && keepAlbumIndex >= 0) {
                g_host->qextAlbumMovieOpen(keepAlbumIndex);
            }

            return -1.0;
        }

        AvioCtx probeState;
        probeState.pos = 0;
        probeState.size = movieSize;

        double probedDuration = -1.0;
        uint8_t *probeBuffer = (uint8_t *)av_malloc(kAvioCapacity);
        AVIOContext *probeAvio = 0;

        if (probeBuffer) {
            probeAvio = avio_alloc_context(probeBuffer, kAvioCapacity, 0, &probeState, AvioRead, NULL, AvioSeek);
        }

        if (probeAvio) {
            AVFormatContext *probeFormat = avformat_alloc_context();

            if (probeFormat) {
                probeFormat->pb = probeAvio;
                probeFormat->probesize = 256 * 1024;
                probeFormat->max_analyze_duration = 0;

                if (avformat_open_input(&probeFormat, NULL, NULL, NULL) == 0) {
                    if (avformat_find_stream_info(probeFormat, NULL) == 0 && probeFormat->duration != AV_NOPTS_VALUE) {
                        probedDuration = probeFormat->duration / (double)AV_TIME_BASE;
                    }

                    avformat_close_input(&probeFormat);
                }
                else {
                    avformat_close_input(&probeFormat);
                }

                av_free(probeAvio->buffer);
            }

            avio_context_free(&probeAvio);
            probeAvio = 0;
            probeBuffer = 0;
        }

        g_host->qextAlbumMovieClose();

        if (wasAlreadyOpen && keepAlbumIndex >= 0) {
            g_host->qextAlbumMovieOpen(keepAlbumIndex);
        }

        return probedDuration;
    }

    /// @brief Probe (and cache) a movie's duration.
    /// @param albumEntryIndex Album entry index.
    /// @return Duration in seconds, or 0 when unknown.
    double Duration(int albumEntryIndex)
    {
        if (albumEntryIndex < 0 || albumEntryIndex >= 256) {
            return -1.0;
        }

        if (s_state.phase != PlaybackPhase::Closed && albumEntryIndex == s_state.albumIndex) {
            return s_state.durationSeconds;
        }

        if (!s_state.durationCacheInit) {
            for (int cacheIndex = 0; cacheIndex < 256; cacheIndex++) {
                s_state.durationCache[cacheIndex] = -1.0;
            }

            s_state.durationCacheInit = true;
        }

        double cachedDuration = s_state.durationCache[albumEntryIndex];

        if (cachedDuration >= 0.0) {
            return cachedDuration;
        }

        if (cachedDuration < -1.5) {
            return -1.0;
        }

        double freshDuration = GetDuration(albumEntryIndex);

        if (freshDuration > 0.0) {
            s_state.durationCache[albumEntryIndex] = freshDuration;
        }
        else {
            s_state.durationCache[albumEntryIndex] = -2.0;
        }

        return freshDuration;
    }

    /// @brief Clear cached durations.
    void DurReset()
    {
        s_state.durationCacheInit = false;
    }

} /* namespace Video */
