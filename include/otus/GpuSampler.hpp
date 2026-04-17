#pragma once
#include "Types.hpp"

namespace otus {

    class GpuSampler {
    public:
        GpuInfo sample() const; // NVML (dlopen) → AMD sysfs → N/A
    private:
        static bool nvidia(GpuInfo& gi);
        static bool amd(GpuInfo& gi);
    };

}
