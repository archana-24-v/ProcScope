#include "procscope_reader.h"
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <cstring>
#include <glob.h>

ProcScopeReader::ProcScopeReader() {}

long ProcScopeReader::hz() {
    static long v = sysconf(_SC_CLK_TCK);
    return v;
}

CpuTotals ProcScopeReader::read_cpu_totals() {
    CpuTotals t;
    std::ifstream f("/proc/stat");
    std::string line;
    if (!f.is_open()) return t;
    while (std::getline(f,line)) {
        if (line.rfind("cpu ",0)==0) {
            std::istringstream iss(line);
            std::string tag;
            iss >> tag >> t.user >> t.nice >> t.system >> t.idle >> t.iowait >> t.irq >> t.softirq >> t.steal;
            break;
        }
    }
    return t;
}

float ProcScopeReader::read_memory_percent() {
    std::ifstream f("/proc/meminfo");
    std::string line;
    unsigned long mem_total=0, mem_available=0;
    while (std::getline(f,line)) {
        if (line.rfind("MemTotal:",0)==0) { std::istringstream iss(line); std::string k; iss>>k>>mem_total; }
        else if (line.rfind("MemAvailable:",0)==0) { std::istringstream iss(line); std::string k; iss>>k>>mem_available; }
        if (mem_total && mem_available) break;
    }
    if (mem_total==0) return 0.0f;
    float used = (float)(mem_total - mem_available);
    return (used / (float)mem_total) * 100.0f;
}

unsigned long long ProcScopeReader::read_uptime_seconds() {
    std::ifstream f("/proc/uptime");
    double up=0.0;
    if (f>>up) return (unsigned long long)up;
    return 0;
}

std::string ProcScopeReader::read_hostname() {
    std::ifstream f("/proc/sys/kernel/hostname");
    std::string s; if (f.is_open()) std::getline(f,s);
    return s;
}

static bool is_number(const char *s) {
    for (; *s; ++s) if (!isdigit(*s)) return false;
    return true;
}

std::vector<ProcInfo> ProcScopeReader::read_processes() {
    std::vector<ProcInfo> out;
    DIR *dp = opendir("/proc");
    if (!dp) return out;
    struct dirent *entry;
    while ((entry = readdir(dp))!=nullptr) {
        if (entry->d_type!=DT_DIR) continue;
        if (!is_number(entry->d_name)) continue;
        int pid = atoi(entry->d_name);
        std::string statpath = std::string("/proc/") + entry->d_name + "/stat";
        std::ifstream sf(statpath);
        if (!sf.is_open()) continue;
        std::string statline; std::getline(sf, statline); sf.close();
        std::istringstream iss(statline);
        ProcInfo pi;
        iss >> pi.pid;
        std::string comm; char state;
        iss >> comm >> state;
        // skip until utime (fields: utime is 14th, stime 15th) -> we've read 3, so skip 10 more
        for (int i=0;i<10;i++) { unsigned long tmp; iss >> tmp; }
        unsigned long utime=0, stime=0;
        iss >> utime >> stime;
        pi.utime = utime; pi.stime = stime; pi.total_ticks = utime + stime;
        // read cmdline
        std::string cmdline = std::string("/proc/") + entry->d_name + "/cmdline";
        std::ifstream cf(cmdline);
        if (cf.is_open()) {
            std::string s; std::getline(cf,s,'\0');
            if (!s.empty()) pi.cmd = s;
        }
        if (pi.cmd.empty()) {
            if (comm.size()>=2 && comm.front()=='(' && comm.back()==')') pi.cmd = comm.substr(1, comm.size()-2);
            else pi.cmd = comm;
        }
        // memory percent via statm
        std::string statmp = std::string("/proc/") + entry->d_name + "/statm";
        std::ifstream sm(statmp);
        if (sm.is_open()) {
            unsigned long size_pages=0, resident=0;
            sm >> size_pages >> resident;
            long page_kb = sysconf(_SC_PAGESIZE)/1024;
            unsigned long rss_kb = resident * page_kb;
            std::ifstream memf("/proc/meminfo"); std::string l2; unsigned long mem_total_kb=0;
            while (std::getline(memf, l2)) {
                if (l2.rfind("MemTotal:",0)==0) { std::istringstream iss2(l2); std::string k; iss2>>k>>mem_total_kb; break; }
            }
            if (mem_total_kb) pi.mem_percent = (float)rss_kb / (float)mem_total_kb * 100.0f;
            sm.close();
        }
        out.push_back(pi);
    }
    closedir(dp);
    return out;
}

bool ProcScopeReader::read_cpu_temperature(float &out_celsius) {
    glob_t g; int ret = glob("/sys/class/thermal/thermal_zone*/temp", 0, nullptr, &g);
    if (ret==0 && g.gl_pathc>0) {
        for (size_t i=0;i<g.gl_pathc;i++) {
            std::ifstream f(g.gl_pathv[i]);
            if (f.is_open()) {
                long val=0; f>>val; f.close();
                if (val>1000) out_celsius = val/1000.0f; else out_celsius = (float)val;
                globfree(&g);
                return true;
            }
        }
        globfree(&g);
    }
    ret = glob("/sys/class/hwmon/*/temp*_input", 0, nullptr, &g);
    if (ret==0 && g.gl_pathc>0) {
        for (size_t i=0;i<g.gl_pathc;i++) {
            std::ifstream f(g.gl_pathv[i]);
            if (f.is_open()) {
                long val=0; f>>val; f.close();
                if (val>1000) out_celsius = val/1000.0f; else out_celsius = (float)val;
                globfree(&g);
                return true;
            }
        }
        globfree(&g);
    }
    return false;
}
