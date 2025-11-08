#include <sys/statvfs.h>
#include "otus/DiskSampler.hpp"

namespace otus {

    DiskUsage DiskSampler::sample(const char* mount) const {
        struct statvfs s{};
        DiskUsage d{};
        if (statvfs(mount, &s)==0) {
            uint64_t total = (uint64_t)s.f_blocks * s.f_frsize;
            uint64_t freeb = (uint64_t)s.f_bfree  * s.f_frsize;
            d.totalBytes = total;
            d.usedBytes  = total - freeb;
        }
        return d;
    }

}
