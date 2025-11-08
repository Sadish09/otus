#include <fstream>
#include <string>
#include "otus/MemSampler.hpp"

namespace otus {

    MemInfo MemSampler::sample() const {
        std::ifstream f("/proc/meminfo");
        MemInfo m{}; std::string key, unit; uint64_t val=0;
        while (f >> key >> val >> unit) {
            if (key=="MemTotal:") m.memTotalKiB = val;
            else if (key=="MemAvailable:") m.memAvailKiB = val;
            else if (key=="SwapTotal:") m.swapTotalKiB = val;
            else if (key=="SwapFree:") m.swapFreeKiB = val;
        }
        return m;
    }
}
