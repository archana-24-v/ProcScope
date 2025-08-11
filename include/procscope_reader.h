#pragma once
#include <string>
#include <vector>

struct ProcInfo {
    int pid = 0;
    std::string cmd;
    unsigned long utime = 0;
    unsigned long stime = 0;
    unsigned long total_ticks = 0;
    float cpu_percent = 0.0f;
    float mem_percent = 0.0f;
};

struct CpuTotals {
    unsigned long long user=0, nice=0, system=0, idle=0, iowait=0, irq=0, softirq=0, steal=0;
    unsigned long long total() const { return user+nice+system+idle+iowait+irq+softirq+steal; }
};

class ProcScopeReader {
public:
    ProcScopeReader();
    CpuTotals read_cpu_totals();
    float read_memory_percent();
    unsigned long long read_uptime_seconds();
    std::vector<ProcInfo> read_processes();
    std::string read_hostname();
    // optional: read cpu temperature (returns true if available)
    bool read_cpu_temperature(float &out_celsius);
private:
    long hz();
};
