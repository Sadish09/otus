#pragma once
#include "Types.hpp"

namespace otus {

    class CpuSampler {
    public:
        double sample();
    private:
        CpuTimes prev_{};
        bool hasPrev_ = false;
        static CpuTimes read_now();
    };

}
