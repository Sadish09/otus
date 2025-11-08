#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace otus {

    struct CpuTimes {
        uint64_t user=0, nice=0, system=0, idle=0, iowait=0, irq=0, softirq=0, steal=0;
        uint64_t total() const { return user+nice+system+idle+iowait+irq+softirq+steal; }
        uint64_t busy()  const { return user+nice+system+irq+softirq+steal; }
    };

    struct MemInfo {
        uint64_t memTotalKiB=0, memAvailKiB=0;
        uint64_t swapTotalKiB=0, swapFreeKiB=0;
    };

    struct DiskUsage {
        uint64_t totalBytes=0, usedBytes=0;
    };

    struct GpuInfo {
        std::string vendor;   // "NVIDIA", "AMD", or ""
        int count=0;
        double utilPct=0.0;
        double memUsedMiB=0.0, memTotalMiB=0.0;
    };

    struct Proc {
        int pid=0, ppid=0;
        char state='R';
        std::string comm, cmdline;
        uint64_t ut=0, st=0, rssPages=0;
        double cpu=0.0;
        size_t memKiB=0;
    };

}
