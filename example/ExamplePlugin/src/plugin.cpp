#include <switch.h>

#include <qlaunch/plugin.h>
#include <qlaunch/titles.h>

/* Provided by the loader */
namespace logging {
    void LogLine(const char *fmt, ...);
}

/* Minimal plugin interface */
extern "C" {
    int onBoot(NWindow *win) {  
        /* run code on boot */
        logging::LogLine("Hello from the example plugin");
        return 0;
    }

    int loop(void) {
        /* run your main loop here. called every tick */
        return 0;
    }

    void onShutdown(void) {
        /* run code on shutdown */
        logging::LogLine("Goodbye from the example plugin");
        return;
    }

    bool IsPersistent(void) {
        /* return true here if you want the plugin to run while in a game */
        return false;
    }
} /* extern C */
