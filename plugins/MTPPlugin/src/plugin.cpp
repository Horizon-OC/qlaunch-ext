/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include <switch.h>
#include <haze.hpp>
#include <haze/console_main_loop.hpp>
#include <haze/ptp_responder.hpp>
#include <haze/event_reactor.hpp>
#include <haze/ptp_object_heap.hpp>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <sys/stat.h>

namespace logging {
void LogLine(const char *fmt, ...);
}

namespace Log {
static void Line(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    logging::LogLine("%s", buf);
}
} /* namespace Log */

namespace mtp {

static Thread s_thread{};
static bool s_haveThread = false;
static std::atomic_bool s_stop{false};
static std::atomic_bool s_exited{true};
static std::atomic<haze::EventReactor *> s_reactor{nullptr};

static haze::Result ServeOnce()
{
    haze::PtpObjectHeap heap;
    haze::EventReactor reactor;
    haze::PtpResponder responder;
    haze::ConsoleMainLoop conloop;
    reactor.SetResult(haze::ResultSuccess());
    R_TRY(conloop.Initialize(&reactor, &heap));
    ON_SCOPE_EXIT { conloop.Finalize(); };
    R_TRY(responder.Initialize(&reactor, &heap));
    ON_SCOPE_EXIT { responder.Finalize(); };
    s_reactor.store(&reactor);
    const haze::Result rc = responder.LoopProcess();
    s_reactor.store(nullptr);
    R_RETURN(rc);
}

static void ServeThread(void *)
{
    while (!s_stop.load()) {
        const haze::Result rc = ServeOnce();
        if (s_stop.load())
            break;
        if (R_FAILED(rc)) {
            Log::Line("[MTP] idle, retrying");
            svcSleepThread(500000000ULL);
        }
    }
    s_exited.store(true);
}

static void StartThread()
{
    if (s_haveThread)
        return;
    s_stop.store(false);
    s_exited.store(false);
    s_reactor.store(nullptr);
    if (threadCreate(&s_thread, ServeThread, nullptr, nullptr, 256 * 1024, 0x3B, -2) != 0) {
        Log::Line("[MTP] threadCreate failed");
        s_exited.store(true);
        return;
    }
    if (threadStart(&s_thread) != 0) {
        Log::Line("[MTP] threadStart failed");
        threadClose(&s_thread);
        s_exited.store(true);
        return;
    }
    s_haveThread = true;
    Log::Line("[MTP] serving SD card");
}

static void StopThread()
{
    s_stop.store(true);
    if (haze::EventReactor *r = s_reactor.load())
        r->SetResult(haze::ResultStopRequested());
    if (s_haveThread) {
        threadWaitForExit(&s_thread);
        threadClose(&s_thread);
        s_haveThread = false;
    }
}

} /* namespace mtp */

#define HOOK __attribute__((visibility("default")))

extern "C" {

HOOK int onBoot(NWindow *win)
{
    (void)win;
    mkdir("sdmc:/qlaunch-ext/config", 0777);
    mkdir("sdmc:/qlaunch-ext/config/plugin", 0777);
    haze::LoadDeviceProperties();
    mtp::StartThread();
    return 0;
}

HOOK int loop(void)
{
    mtp::StartThread();
    return 0;
}

HOOK void onShutdown(void)
{
    mtp::StopThread();
}

HOOK void onPowerButton(void)
{
    mtp::StopThread();
}

HOOK bool IsPersistent(void)
{
    return true;
}

} /* extern C */
