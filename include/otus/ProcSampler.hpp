#pragma once
#include <unordered_map>
#include <utility>
#include <vector>
#include "Types.hpp"

namespace otus {

    class ProcSampler {
    public:
        std::vector<Proc> sample(double dtSeconds); // per process CPU%
    private:
        const long hertz_ = sysconf(_SC_CLK_TCK);
        std::unordered_map<int, std::pair<uint64_t,uint64_t>> prevTimes_;

        static bool parse_stat(const std::string& s, std::string& comm, char& st, int& ppid,
                               uint64_t& ut, uint64_t& stt, uint64_t& rss);
    };
}
