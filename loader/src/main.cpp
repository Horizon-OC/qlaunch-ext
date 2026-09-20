#include <shared/logging.hpp>
#include <switch.h>

extern "C" {
    u32 __nx_applet_type = AppletType_SystemApplet;
    u32 __nx_fs_num_sessions = 3;

    size_t __nx_heap_size = 0x800000;
    TimeServiceType __nx_time_service_type = TimeServiceType_System;
}

extern "C" void __appInit(void) {
    smInitialize();
    fsInitialize();
    appletInitialize();
    timeInitialize();
    setsysInitialize();
    setInitialize();

    SetSysFirmwareVersion fw = {};
    hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro) | BIT(31));

    nsInitialize();
    ldrShellInitialize();
    accountInitialize();
    nssuInitialize();
    avmInitialize();
    psmInitialize();
    lblInitialize();
    hidInitialize();
    fsdevMountSdmc();
    logging::Initialize();
    logging::SetLogOutput(logging::LogOutput_File);
    logging::SetFileLoggingPath("sdmc:/qlaunch-ext-log.txt");
    logging::LogLine("[qlaunch-ext] Initalized services")
}

extern "C" void __appExit(void) {
    logging::Exit();

    hidExit();
    lblExit();
    psmExit();
    avmExit();
    nssuExit();
    accountExit();
    ldrShellExit();
    nsExit();
    setExit();
    setsysExit();
    timeExit();
    appletExit();
    fsdevUnmountAll();
    fsExit();
    smExit();
}

