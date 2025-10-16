// Lightweight system memory helpers (no logic changes)
#pragma once

#include <cstddef>

#ifdef _WIN32
#include <windows.h>
inline size_t getAvailableMemory() {
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        return statex.ullAvailPhys;
    }
    return 0;
}
#else
#include <sys/sysinfo.h>
inline size_t getAvailableMemory() {
    struct sysinfo memInfo;
    if (sysinfo(&memInfo) == 0) {
        return memInfo.freeram * memInfo.mem_unit;
    }
    return 0;
}
#endif

