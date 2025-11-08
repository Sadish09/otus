#include <fstream>
#include <string>
#include "otus/CpuSampler.hpp"

namespace otus {

    CpuTimes CpuSampler::read_now() {
        std::ifstream f("/proc/stat"); // Poll CPU stats from a quick syscall to VFS
        CpuTimes c{}; std::string tag;
        if (f && (f>>tag) && tag=="cpu")
            f >> c.user >> c.nice >> c.system >> c.idle >> c.iowait >> c.irq >> c.softirq >> c.steal;
        return c;

        // TODO: Add sysfs frequency and per core usage later on
    }

    double CpuSampler::sample() {
        CpuTimes cur = read_now();
        double pct = 0.0;
        if (hasPrev_) {
            uint64_t dtot = cur.total() - prev_.total();
            uint64_t dbus = cur.busy()  - prev_.busy();
            if (dtot) pct = (double)dbus * 100.0 / (double)dtot;
        }
        prev_ = cur; hasPrev_ = true;
        return pct;
    }

}
