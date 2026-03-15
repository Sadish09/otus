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

//quit flag
static bool g_run = true;
static void on_sigint(int) { g_run = false; }

//raw terminal so keypresses don't need Enter
struct StdinRaw {
    termios orig{};
    bool active = false;

    StdinRaw() {
        if (!isatty(STDIN_FILENO)) return;
        if (tcgetattr(STDIN_FILENO, &orig) != 0) return;
        termios raw = orig;
        cfmakeraw(&raw);
        raw.c_lflag |= ISIG;  // keep Ctrl-C delivering SIGINT
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        active = true;
    }
    ~StdinRaw() {
        if (active) tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    }
};

// Non-blocking key read - flips O_NONBLOCK only for this one read, then restores
static int read_key() {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    unsigned char c = 0;
    ssize_t n = ::read(STDIN_FILENO, &c, 1);
    fcntl(STDIN_FILENO, F_SETFL, flags);
    return (n == 1) ? (int)c : -1;
}

// Sleep N seconds in 50ms chunks - quits early on q/ESC/Ctrl-C
static void interruptible_sleep(int seconds) {
    for (int i = 0; i < seconds * 20 && g_run; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int k = read_key();
        if (k == 'q' || k == 'Q' || k == 27 || k == 3) g_run = false;
    }
}

//options
struct Options {
    bool cpu=false, gpu=false, mem=false, proc=false;
    int procLimit=40;
    int intervalSec=1;
};

void print_help(const char* prog) {
    std::cout <<
"otus — system monitor\n\n"
"USAGE:\n"
"  " << prog << "             combined dashboard\n"
"  " << prog << " -cpu        CPU% only\n"
"  " << prog << " -gpu        GPU summary\n"
"  " << prog << " -mem        memory usage\n"
"  " << prog << " -proc       process tree only\n"
"  " << prog << " -proc -lim N  limit process nodes (default 40)\n"
"  " << prog << " -i SEC      refresh interval (default 1)\n"
"  " << prog << " --help\n\n"
"Press q / ESC / Ctrl-C to quit.\n";
}

Options parse_args(int argc, char** argv) {
    Options o{};
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--help" || a == "-h") { print_help(argv[0]); std::exit(0); }
        else if (a == "-cpu")  o.cpu  = true;
        else if (a == "-gpu")  o.gpu  = true;
        else if (a == "-mem")  o.mem  = true;
        else if (a == "-proc") o.proc = true;
        else if (a == "-lim" && i+1 < argc) {
            char* end = nullptr;
            long v = std::strtol(argv[++i], &end, 10);
            if (*end || v <= 0) { std::cerr << "Invalid -lim value\n"; std::exit(2); }
            o.procLimit = (int)v;
        }
        else if (a == "-i" && i+1 < argc) {
            char* end = nullptr;
            long v = std::strtol(argv[++i], &end, 10);
            if (*end || v <= 0) { std::cerr << "Invalid -i value\n"; std::exit(2); }
            o.intervalSec = (int)v;
        }
    }
    return o;
}

//formatting
std::string fmt1(double v) {
    std::ostringstream ss; ss.setf(std::ios::fixed); ss.precision(1); ss << v; return ss.str();
}
std::string gib_bytes(uint64_t b) { return fmt1(b / 1073741824.0) + " GiB"; }

// ── UI helpers ───
ftxui::Element gauge_labeled(const string& label, double ratio, const string& right) {
    if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1;
    return hbox(
        text(label) | dim | size(WIDTH, EQUAL, 5),
        gauge(ratio) | flex,
        text(" " + right) | dim | size(WIDTH, EQUAL, 20)
    );
}

// ── process tree ─
std::vector<int> build_roots(const std::vector<otus::Proc>& ps) {
    std::set<int> have;
    for (auto& p : ps) have.insert(p.pid);
    std::unordered_map<int,double> cp;
    for (auto& p : ps) cp[p.pid] = p.cpu;
    std::vector<int> roots;
    for (auto& p : ps)
        if (!have.count(p.ppid) || p.pid == 1) roots.push_back(p.pid);
    std::sort(roots.begin(), roots.end(), [&](int a, int b){ return cp[a] > cp[b]; });
    return roots;
}

std::unordered_map<int,std::vector<int>> build_children(const std::vector<otus::Proc>& ps) {
    std::unordered_map<int,std::vector<int>> ch;
    std::unordered_map<int,double> cp;
    for (auto& p : ps) cp[p.pid] = p.cpu;
    for (auto& p : ps) ch[p.ppid].push_back(p.pid);
    for (auto& kv : ch)
        std::sort(kv.second.begin(), kv.second.end(), [&](int a, int b){ return cp[a] > cp[b]; });
    return ch;
}

const otus::Proc* findp(const std::vector<otus::Proc>& v, int pid) {
    for (auto& p : v) if (p.pid == pid) return &p;
    return nullptr;
}

std::function<Element(const std::vector<otus::Proc>&,
                      const std::unordered_map<int,std::vector<int>>&,
                      int, int&, int, int)> build_node =
[](const std::vector<otus::Proc>& ps,
   const std::unordered_map<int,std::vector<int>>& ch,
   int pid, int& printed, int limit, int depth) -> Element {

    if (printed >= limit) return Element{};
    auto p = findp(ps, pid);
    if (!p) return Element{};

    std::ostringstream right;
    right.setf(std::ios::fixed); right.precision(1);
    right << fmt1(p->cpu) << "%  " << fmt1(p->memKiB/1024.0) << "M  [" << p->state << "]";

    Element row = hbox(
        text(std::string(depth*2, ' ')),
        text(std::to_string(p->pid) + "  " + (p->cmdline.empty() ? p->comm : p->cmdline)) | flex,
        text(right.str()) | dim | size(WIDTH, EQUAL, 24)
    );
    printed++;

    auto it = ch.find(pid);
    if (it == ch.end()) return row;

    Elements kids;
    for (int cpid : it->second) {
        if (printed >= limit) break;
        auto kid = build_node(ps, ch, cpid, printed, limit, depth+1);
        if (kid) kids.push_back(std::move(kid));
    }
    if (kids.empty()) return row;
    return vbox({ row, vbox(std::move(kids)) });
};

//main
int main(int argc, char** argv) {
    auto opt = parse_args(argc, argv);
    std::signal(SIGINT, on_sigint);
    StdinRaw raw_guard;

    otus::CpuSampler  cpu;
    otus::MemSampler  mem;
    otus::DiskSampler disk;
    otus::GpuSampler  gpu;
    otus::ProcSampler procs;

    //single-stat modes
    if (opt.cpu && !opt.gpu && !opt.mem && !opt.proc) {
        while (g_run) {
            double c = cpu.sample();
            std::cout << "\033[2K\rCPU  " << fmt1(c) << "%" << std::flush;
            interruptible_sleep(opt.intervalSec);
        }
        std::cout << "\n"; return 0;
    }

    if (opt.gpu && !opt.cpu && !opt.mem && !opt.proc) {
        while (g_run) {
            auto g = gpu.sample();
            std::ostringstream ss;
            if (!g.count) {
                ss << "GPU  N/A";
            } else {
                ss << "GPU  " << g.vendor << " x" << g.count
                   << "  " << (int)g.utilPct << "%";
                if (g.memTotalMiB > 0)
                    ss << "  " << fmt1(g.memUsedMiB/1024.0)
                       << "/" << fmt1(g.memTotalMiB/1024.0) << "G";
            }
            std::cout << "\033[2K\r" << ss.str() << std::flush;
            interruptible_sleep(opt.intervalSec);
        }
        std::cout << "\n"; return 0;
    }

    if (opt.mem && !opt.cpu && !opt.gpu && !opt.proc) {
        while (g_run) {
            auto m = mem.sample();
            double used = (m.memTotalKiB - m.memAvailKiB) / 1048576.0;
            double tot  = m.memTotalKiB / 1048576.0;
            std::cout << "\033[2K\rMEM  " << fmt1(used) << " / " << fmt1(tot)
                      << " GiB  (" << fmt1(used/tot*100) << "%)" << std::flush;
            interruptible_sleep(opt.intervalSec);
        }
        std::cout << "\n"; return 0;
    }

    //FTXUI modes
    std::cout << "\033[?25l"; // hide cursor while rendering

    if (opt.proc && !opt.cpu && !opt.gpu && !opt.mem) {
        while (g_run) {
            auto plist = procs.sample(opt.intervalSec);
            auto roots = build_roots(plist);
            auto ch    = build_children(plist);
            int printed = 0;
            Elements groups;
            for (int r : roots) {
                if (printed >= opt.procLimit) break;
                auto e = build_node(plist, ch, r, printed, opt.procLimit, 0);
                if (e) groups.push_back(std::move(e));
            }
            auto doc = window(text(" processes ") | bold, vbox(std::move(groups))) | border;
            auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(doc));
            Render(screen, doc); screen.Print();
            std::cout << "\033[H" << std::flush;
            interruptible_sleep(opt.intervalSec);
        }
        std::cout << "\033[?25h\033[2J\033[H"; return 0;
    }

    // ── dashboard 
    while (g_run) {
        double c = cpu.sample();
        auto m = mem.sample();
        auto d = disk.sample("/");
        auto g = gpu.sample();

        double mem_ratio  = m.memTotalKiB ? (double)(m.memTotalKiB - m.memAvailKiB) / m.memTotalKiB : 0.0;
        double disk_ratio = d.totalBytes  ? (double)d.usedBytes / d.totalBytes : 0.0;

        string gpu_right;
        if (g.count) {
            std::ostringstream gs; gs.setf(std::ios::fixed); gs.precision(1);
            gs << (int)g.utilPct << "%";
            if (g.memTotalMiB > 0)
                gs << "  " << fmt1(g.memUsedMiB/1024.0) << "/" << fmt1(g.memTotalMiB/1024.0) << "G";
            gpu_right = gs.str();
        }

        auto stats = window(text(" otus ") | bold,
            vbox({
                gauge_labeled("CPU  ", c/100.0, fmt1(c) + "%"),
                gauge_labeled("MEM  ", mem_ratio,
                    fmt1((m.memTotalKiB-m.memAvailKiB)/1048576.0) + "/" + fmt1(m.memTotalKiB/1048576.0) + " GiB"),
                gauge_labeled("DSK  ", disk_ratio,
                    gib_bytes(d.usedBytes) + "/" + gib_bytes(d.totalBytes)),
                separator(),
                hbox({
                    text("GPU  ") | dim,
                    text(g.count ? (g.vendor + " ×" + std::to_string(g.count)) : "N/A"),
                    filler(),
                    text(gpu_right) | dim,
                }),
            })
        ) | border;

        auto plist = procs.sample(opt.intervalSec);
        auto roots = build_roots(plist);
        auto ch    = build_children(plist);
        int printed = 0;
        Elements groups;
        for (int r : roots) {
            if (printed >= opt.procLimit) break;
            auto e = build_node(plist, ch, r, printed, opt.procLimit, 0);
            if (e) groups.push_back(std::move(e));
        }

        auto proc_panel = window(text(" processes ") | bold, vbox(std::move(groups))) | border;
        auto layout = vbox({ stats, proc_panel | size(HEIGHT, LESS_THAN, 40) });

        auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(layout));
        Render(screen, layout); screen.Print();
        std::cout << "\033[H" << std::flush;
        interruptible_sleep(opt.intervalSec);
    }

    std::cout << "\033[?25h\033[2J\033[H"; return 0; //ANSI escape codes: https://gist.github.com/ConnerWill/d4b6c776b509add763e17f9f113fd25b
}