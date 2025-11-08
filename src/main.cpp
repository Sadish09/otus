#include <csignal>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <set>
#include <sstream>
#include <iostream>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include "otus/CpuSampler.hpp"
#include "otus/MemSampler.hpp"
#include "otus/DiskSampler.hpp"
#include "otus/GpuSampler.hpp"
#include "otus/ProcSampler.hpp"

using namespace ftxui;
using std::string;

using ftxui::Element;
using ftxui::Elements;
using ftxui::text;
using ftxui::hbox;
using ftxui::vbox;
using ftxui::nothing;
using ftxui::dim;
using ftxui::flex;

namespace {

struct Options {
    bool cpu=false, gpu=false, mem=false, proc=false;
    int procLimit=40;
    int intervalSec=1;
};

void print_help(const char* prog){
    std::cout <<
R"(otus — System monitor but prettier! (built with FTXUI)

USAGE:
  )" << prog << R"(             # combined: CPU/GPU/MEM/DISK + grouped process tree
  )" << prog << R"( -cpu        # CPU% only (one number updated every sec)
  )" << prog << R"( -gpu        # GPU summary of whatever is supported, might return empty
  )" << prog << R"( -mem        # memory usage (used/total GiB)
  )" << prog << R"( -proc       # process tree only
  )" << prog << R"( -proc -lim N # limit process nodes to N (default 40)
  )" << prog << R"( -i SEC      # refresh interval (default 1)
  )" << prog << R"( --help

)";
}

Options parse_args(const int argc, char** argv) {
    Options o{};
    for (int i=1;i<argc;i++){
        string a = argv[i];
        if (a=="--help" || a=="-h") { print_help(argv[0]); std::exit(0); }
        else if (a=="-cpu")  o.cpu  = true;
        else if (a=="-gpu")  o.gpu  = true;
        else if (a=="-mem")  o.mem  = true;
        else if (a=="-proc") o.proc = true;
        else if (a == "-lim" && i+1 < argc) {
            char* end = nullptr;
            long v = std::strtol(argv[++i], &end, 10);
            if (*end == '\0' && v > 0) {
                o.procLimit = static_cast<int>(v);
            } else {
                std::cerr << "Invalid value for -lim: " << argv[i] << "\n";
                std::exit(2);
            }
        }
        else if (a == "-i" && i+1 < argc) {
            char* end = nullptr;
            long v = std::strtol(argv[++i], &end, 10);
            if (*end == '\0' && v > 0) {
                o.intervalSec = static_cast<int>(v);
            } else {
                std::cerr << "Invalid value for -i: " << argv[i] << "\n";
                std::exit(2);
            }
        }
    }
    return o;
}

std::string gib_bytes(uint64_t b) {
    std::ostringstream ss; ss.setf(std::ios::fixed); ss.precision(1);
    ss << (b/1073741824.0) << " GiB"; return ss.str();
}

ftxui::Element gauge_labeled(const string& label, double ratio, const string& right){
    if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1;
    return hbox(text(label) | dim | size(WIDTH,EQUAL,7),
                gauge(ratio) | flex,
                text(" " + right) | dim);
}
}

static bool g_run = true;
static void on_sigint(int){ g_run=false; }

struct StdinRaw { // A little RAII helper
    termios orig{};
    int orig_flags = -1;
    bool active = false;

    StdinRaw() {
        if (!isatty(STDIN_FILENO)) return;
        if (tcgetattr(STDIN_FILENO, &orig) != 0) return;
        termios raw = orig;
        cfmakeraw(&raw);
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return;
        orig_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, orig_flags | O_NONBLOCK);
        active = true;
    }
    ~StdinRaw() {
        if (!active) return;
        tcsetattr(STDIN_FILENO, TCSANOW, &orig);
        if (orig_flags != -1) fcntl(STDIN_FILENO, F_SETFL, orig_flags);
    }
};

// returns -1 if no key; otherwise ASCII code
static int read_key_nonblock() {
    unsigned char c;
    ssize_t n = ::read(STDIN_FILENO, &c, 1);
    return (n == 1) ? (int)c : -1;
}


int main(int argc, char** argv){
    auto opt = parse_args(argc, argv);
    std::signal(SIGINT, on_sigint);
    StdinRaw raw_mode_guard;

    otus::CpuSampler  cpu;
    otus::MemSampler  mem;
    otus::DiskSampler disk;
    otus::GpuSampler  gpu;
    otus::ProcSampler procs;

    // Individual Mode, QML frontend work for KDE Plasma is underway
    if ( opt.cpu && !opt.gpu && !opt.mem && !opt.proc ){
        while (g_run) {
            double c = cpu.sample();
            std::cout.setf(std::ios::fixed); std::cout.precision(1);
            std::cout << c << "\r" << std::flush;
            std::this_thread::sleep_for(std::chrono::seconds(opt.intervalSec));

            if (int k = read_key_nonblock(); k == 'q' || k == 'Q' || k == 27 || k == 3) {
                g_run = false;
            }
        }
        std::cout << "\n"; return 0;
    }
    if ( opt.gpu && !opt.cpu && !opt.mem && !opt.proc ){
        while (g_run) {
            auto g = gpu.sample();
            std::ostringstream ss; ss.setf(std::ios::fixed); ss.precision(1);
            if (!g.count) ss << "N/A";
            else {
                ss << g.vendor << " x" << g.count << " " << (int)g.utilPct << "%";
                if (g.memTotalMiB>0) ss << "  " << (g.memUsedMiB/1024.0) << " / " << (g.memTotalMiB/1024.0) << " GiB";
            }
            std::cout << ss.str() << "\r" << std::flush;
            std::this_thread::sleep_for(std::chrono::seconds(opt.intervalSec));

            if (int k = read_key_nonblock(); k == 'q' || k == 'Q' || k == 27 || k == 3) {
                g_run = false;
            }
        }
        std::cout << "\n"; return 0;
    }
    if ( opt.mem && !opt.cpu && !opt.gpu && !opt.proc ){
        while (g_run) {
            auto m = mem.sample();
            double used = (m.memTotalKiB - m.memAvailKiB)/1048576.0;
            double tot  = m.memTotalKiB/1048576.0;
            std::cout.setf(std::ios::fixed); std::cout.precision(1);
            std::cout << used << " / " << tot << " GiB\r" << std::flush;
            std::this_thread::sleep_for(std::chrono::seconds(opt.intervalSec));

            if (int k = read_key_nonblock(); k == 'q' || k == 'Q' || k == 27 || k == 3) {
                g_run = false;
            }
        }
        std::cout << "\n"; return 0;
    }

    auto build_roots = [](const std::vector<otus::Proc>& ps){
        std::set<int> have; for (auto& p: ps) have.insert(p.pid);
        std::vector<int> roots;
        for (auto& p: ps) if (!have.count(p.ppid) || p.pid==1) roots.push_back(p.pid);
        std::unordered_map<int,double> cp; for (auto& p: ps) cp[p.pid]=p.cpu;
        std::sort(roots.begin(), roots.end(), [&](int a,int b){ return cp[a]>cp[b]; });
        return roots;
    };
    auto build_children = [](const std::vector<otus::Proc>& ps){
        std::unordered_map<int,std::vector<int>> ch;
        std::unordered_map<int,double> cp; for (auto& p: ps) cp[p.pid]=p.cpu;
        for (auto& p: ps) ch[p.ppid].push_back(p.pid);
        for (auto& kv: ch) std::sort(kv.second.begin(), kv.second.end(), [&](int a,int b){ return cp[a]>cp[b]; });
        return ch;
    };
    auto findp = [](const std::vector<otus::Proc>& v, int pid)->const otus::Proc*{
        for (auto& p: v) if (p.pid==pid) return &p; return nullptr;
    };


    std::function<Element(const std::vector<otus::Proc>&,
                          const std::unordered_map<int,std::vector<int>>&,
                          int,int&,int,int)> build_node =
    [&](const std::vector<otus::Proc>& ps,
        const std::unordered_map<int,std::vector<int>>& ch,
        int pid, int& printed, int limit, int depth) -> Element {

        if (printed >= limit) return Element{};

        auto p = findp(ps, pid);
        if (!p) return ftxui::Element{};

        std::ostringstream right; right.setf(std::ios::fixed); right.precision(1);
        right << "CPU " << p->cpu << "%  RSS " << (p->memKiB/1024.0) << " MiB  [" << p->state << "]";

        // one row for this process
        Element row = hbox(
            text(std::string(depth*2, ' ')),
            hbox(
                text(std::to_string(p->pid) + "  " + (p->cmdline.empty()? p->comm : p->cmdline)) | flex,
                text(right.str()) | dim
            )
        );
        printed++;

        // children
        auto it = ch.find(pid);
        if (it == ch.end()) return row;

        Elements kids;
        for (int cpid : it->second) {
            if (printed >= limit) break;
            kids.push_back(build_node(ps, ch, cpid, printed, limit, depth+1));
        }

        return vbox({
            row,
            vbox(std::move(kids))
        });
    };

    // Process-only FTXUI
    if ( opt.proc && !opt.cpu && !opt.gpu && !opt.mem ){
        while (g_run) {
            auto plist = procs.sample(opt.intervalSec);
            auto roots = build_roots(plist);
            auto ch    = build_children(plist);

            int printed=0; std::vector<std::shared_ptr<Node>> groups;
            for (int r : roots) {
                if (printed>=opt.procLimit) break;
                groups.push_back(build_node(plist, ch, r, printed, opt.procLimit,0));
            }
            auto doc = window(text(" processes ") | bold, vbox((groups))) | border;
            auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(doc));
            Render(screen, doc); screen.Print();
            std::cout << "\033[H";
            std::this_thread::sleep_for(std::chrono::seconds(opt.intervalSec));

            if (int k = read_key_nonblock(); k == 'q' || k == 'Q' || k == 27 || k == 3) {
                g_run = false;
            }
        }
        return 0;
    }

    // Dashboard (everything)
    while (g_run) {
        double c = cpu.sample();
        auto m = mem.sample();
        auto d = disk.sample("/");
        auto g = gpu.sample();

        double mem_ratio  = m.memTotalKiB ? (double)(m.memTotalKiB - m.memAvailKiB) / (double)m.memTotalKiB : 0.0;
        double disk_ratio = d.totalBytes ? (double)d.usedBytes / (double)d.totalBytes : 0.0;

        auto title = window(text(" otus ") | bold,
            vbox({
                gauge_labeled("CPU", c/100.0, [&]{ std::ostringstream ss; ss.setf(std::ios::fixed); ss.precision(1); ss<<c<<"%"; return ss.str(); }()),
                gauge_labeled("MEM", mem_ratio, [&]{ std::ostringstream ss; ss.setf(std::ios::fixed); ss.precision(1);
                                                            ss << ((m.memTotalKiB-m.memAvailKiB)/1048576.0) << " / " << (m.memTotalKiB/1048576.0) << " GiB"; return ss.str(); }()),
                gauge_labeled("DSK", disk_ratio, gib_bytes(d.usedBytes) + " / " + gib_bytes(d.totalBytes)),
                hbox({
                    text("GPU ") | dim,
                    text(g.count ? (g.vendor + " x" + std::to_string(g.count)) : "N/A"),
                    filler(),
                    text(g.count ? (std::to_string((int)g.utilPct) + "%  " +
                        (g.memTotalMiB>0 ? ([&]{ std::ostringstream ss; ss.setf(std::ios::fixed); ss.precision(1);
                            ss << (g.memUsedMiB/1024.0) << " / " << (g.memTotalMiB/1024.0) << " GiB"; return ss.str(); }()) : "")) : "") | dim
                })
            })
        ) | border;

        // Processes
        auto plist = procs.sample(opt.intervalSec);
        auto roots = build_roots(plist);
        auto ch    = build_children(plist);
        int printed=0; std::vector<std::shared_ptr<Node>> groups;
        for (int r : roots) {
            if (printed>=opt.procLimit) break;
            groups.push_back(build_node(plist, ch, r, printed, opt.procLimit,0));
        }
        auto proc_panel = window(text(" processes ") | bold,
                                 vbox(groups)) | border;


        auto layout = vbox({
            title,
            separator(),
            proc_panel | size(HEIGHT, LESS_THAN, 40)
        });

        auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(layout));
        Render(screen, layout); screen.Print();
        std::cout << "\033[H";
        std::this_thread::sleep_for(std::chrono::seconds(opt.intervalSec));

        if (int k = read_key_nonblock(); k == 'q' || k == 'Q' || k == 27 || k == 3) {
            g_run = false;
        }
    }
    return 0;
}
