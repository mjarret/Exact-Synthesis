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
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <unistd.h>
inline size_t getAvailableMemory() {
    int64_t freeBytes = 0;
    size_t len = sizeof(freeBytes);
    if (sysctlbyname("hw.memsize", &freeBytes, &len, nullptr, 0) == 0)
        return static_cast<size_t>(freeBytes);
    return 0;
}
inline size_t getProcessRSSBytes() {
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
        return info.resident_size;
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
