//
// Created by sadish-kumar on 07/11/25.
//

#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstdint>

namespace otus {

    inline bool read_all(const std::string& path, std::string& out) {
        std::ifstream f(path);
        if (!f) return false;
        std::ostringstream ss; ss << f.rdbuf();
        out = ss.str(); return true;
    }

    inline uint64_t to_u64(const std::string& s) {
        try { return std::stoull(s); } catch (...) { return 0ULL; }
    }

    inline bool read_sysfs_u64_trim(const std::string& path, uint64_t& out) {
        std::string s; if (!read_all(path, s)) return false;
        size_t i=0; while (i<s.size() && std::isspace((unsigned char)s[i])) ++i;
        size_t j=s.size(); while (j>i && std::isspace((unsigned char)s[j-1])) --j;
        out = to_u64(s.substr(i, j-i));
        return true;
    }

}
