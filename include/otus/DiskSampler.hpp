#pragma once
#include "Types.hpp"

namespace otus {

    class DiskSampler {
    public:
        DiskUsage sample(const char* mount = "/") const;
    };

}
