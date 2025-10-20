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
#include <fstream>
#include <unistd.h>
inline size_t getAvailableMemory() {
    struct sysinfo memInfo;
    if (sysinfo(&memInfo) == 0) {
        return memInfo.freeram * memInfo.mem_unit;
    }
    return 0;
}

// Process resident set size (RSS) in bytes (Linux). Returns 0 on failure.
inline size_t getProcessRSSBytes() {
    long resident_pages = 0;
    std::ifstream statm("/proc/self/statm");
    if (statm.good()) {
        long size_pages = 0; // unused
        statm >> size_pages >> resident_pages;
    }
    long page_size = sysconf(_SC_PAGESIZE);
    if (resident_pages <= 0 || page_size <= 0) return 0;
    return static_cast<size_t>(resident_pages) * static_cast<size_t>(page_size);
}
#endif
