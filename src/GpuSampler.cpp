#include <filesystem>
#include <dlfcn.h>
#include "otus/GpuSampler.hpp"
#include "otus/Helpers.hpp"

namespace fs = std::filesystem;

namespace otus {

struct NvmlUtil { unsigned int gpu; unsigned int memory; };
struct NvmlMem  { unsigned long long total, free, used; };
using F0 = int(*)(); using F1u = int(*)(unsigned int*);
using F1uiV = int(*)(unsigned int, void**);
using FUtil = int(*)(void*, NvmlUtil*);
using FMem  = int(*)(void*, NvmlMem*);

bool GpuSampler::nvidia(GpuInfo& gi) {
    void* h = dlopen("libnvidia-ml.so.1", RTLD_LAZY);
    if (!h) return false;
    auto nvmlInit  = (F0)dlsym(h, "nvmlInit_v2"); if(!nvmlInit) nvmlInit=(F0)dlsym(h,"nvmlInit");
    auto nvmlShutdown=(F0)dlsym(h, "nvmlShutdown");
    auto nvmlCount = (F1u)dlsym(h, "nvmlDeviceGetCount_v2");
    auto nvmlByIdx = (F1uiV)dlsym(h, "nvmlDeviceGetHandleByIndex_v2");
    auto nvmlUtil  = (FUtil)dlsym(h, "nvmlDeviceGetUtilizationRates");
    auto nvmlMem   = (FMem)dlsym(h, "nvmlDeviceGetMemoryInfo");
    if (!nvmlInit||!nvmlShutdown||!nvmlCount||!nvmlByIdx||!nvmlUtil||!nvmlMem) { dlclose(h); return false; }

    if (nvmlInit()!=0) { dlclose(h); return false; }
    unsigned int count=0; if (nvmlCount(&count)!=0 || !count) { nvmlShutdown(); dlclose(h); return false; }

    gi.vendor="NVIDIA"; gi.count=(int)count;
    double util=0.0, used=0.0, tot=0.0;
    for (unsigned i=0;i<count;++i) {
        void* dev=nullptr; if (nvmlByIdx(i,&dev)!=0) continue;
        NvmlUtil u{}; if (nvmlUtil(dev,&u)==0) util += u.gpu;
        NvmlMem  m{}; if (nvmlMem(dev,&m)==0) { tot+=m.total/1048576.0; used+=m.used/1048576.0; }
    }
    gi.utilPct = util / count; gi.memTotalMiB=tot; gi.memUsedMiB=used;
    nvmlShutdown(); dlclose(h);
    return true;
}

bool GpuSampler::amd(GpuInfo& gi) {
    std::vector<fs::path> cards;
    for (auto& p : fs::directory_iterator("/sys/class/drm")) {
        auto n = p.path().filename().string();
        if (n.rfind("card",0)==0) {
            std::string vendor;
            if (read_all(p.path().string()+"/device/vendor", vendor) && vendor.find("0x1002")!=std::string::npos)
                cards.push_back(p.path());
        }
    }
    if (cards.empty()) return false;

    gi.vendor="AMD"; gi.count=(int)cards.size();
    double util=0.0, used=0.0, tot=0.0;
    for (auto& c : cards) {
        uint64_t busy=0, vtot=0, vuse=0;
        if (read_sysfs_u64_trim(c.string()+"/device/gpu_busy_percent", busy)) util += busy;
        if (read_sysfs_u64_trim(c.string()+"/device/mem_info_vram_total", vtot)) tot += vtot/1048576.0;
        if (read_sysfs_u64_trim(c.string()+"/device/mem_info_vram_used",  vuse)) used += vuse/1048576.0;
    }
    gi.utilPct = util / gi.count; gi.memTotalMiB=tot; gi.memUsedMiB=used;
    return true;
}

GpuInfo GpuSampler::sample() const {
    GpuInfo gi; if (nvidia(gi)) return gi; if (amd(gi)) return gi; return gi;
}

}
