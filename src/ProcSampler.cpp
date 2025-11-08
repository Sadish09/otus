#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include "otus/ProcSampler.hpp"
#include "otus/Helpers.hpp"

namespace fs = std::filesystem;

namespace otus {

bool ProcSampler::parse_stat(const std::string& s, std::string& comm, char& st, int& ppid,
                             uint64_t& ut, uint64_t& stt, uint64_t& rss) {
    size_t l = s.find('('), r = s.rfind(')');
    if (l==std::string::npos || r==std::string::npos || r<=l) return false;
    comm = s.substr(l+1, r-l-1);
    std::string after = s.substr(r+2);
    std::vector<std::string> v; std::string tok; std::stringstream ss(after);
    while (ss >> tok) v.push_back(tok);
    if (v.size() < 24) return false;
    st   = v[0][0];
    ppid = std::stoi(v[1]);
    ut   = to_u64(v[11]);
    stt  = to_u64(v[12]);
    rss  = to_u64(v[22]);
    return true;
}

std::vector<Proc> ProcSampler::sample(double dtSeconds) {
    std::vector<Proc> v;
    for (auto& e : fs::directory_iterator("/proc")) {
        if (!e.is_directory()) continue;
        auto name = e.path().filename().string();
        if (!std::all_of(name.begin(), name.end(), ::isdigit)) continue;
        int pid = std::stoi(name);

        std::string stat; if (!read_all(e.path().string()+"/stat", stat)) continue;
        std::string comm; char state='R'; int ppid=0; uint64_t ut=0, st=0, rss=0;
        if (!parse_stat(stat, comm, state, ppid, ut, st, rss)) continue;

        std::string cmd;
        { std::ifstream f(e.path().string()+"/cmdline"); if (f) { std::string a; bool first=true;
          while (std::getline(f, a, '\0')) { if (!a.empty()) { if (!first) cmd+=' '; cmd+=a; first=false; } } } }

        Proc p; p.pid=pid; p.ppid=ppid; p.state=state; p.comm=comm; p.cmdline=cmd;
        p.ut=ut; p.st=st; p.rssPages=rss;
        long page = sysconf(_SC_PAGESIZE);
        p.memKiB = (size_t)((rss * (uint64_t)page) / 1024);
        v.push_back(std::move(p));
    }
    // CPU% from deltas
    for (auto& p : v) {
        auto it = prevTimes_.find(p.pid);
        if (it==prevTimes_.end() || dtSeconds<=0.0) p.cpu=0.0;
        else {
            double d = (double)((p.ut - it->second.first) + (p.st - it->second.second));
            p.cpu = (d / (hertz_ * dtSeconds)) * 100.0;
            if (p.cpu < 0) p.cpu = 0.0;
        }
    }
    // refresh cache
    std::unordered_map<int, std::pair<uint64_t,uint64_t>> cur;
    for (auto& p : v) cur[p.pid] = {p.ut, p.st};
    prevTimes_.swap(cur);
    return v;
}

}
