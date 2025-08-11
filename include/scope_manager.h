#pragma once
#include "procscope_reader.h"
#include <vector>
#include <mutex>
#include <string>

struct ScopeSnapshot {
    CpuTotals cpu;
    float cpu_percent = 0.0f;
    float mem_percent = 0.0f;
    unsigned long long uptime_seconds = 0;
    std::string hostname;
    float cpu_temp_c = -1000.0f; // sentinel for missing
    std::vector<ProcInfo> procs;
};

class ScopeManager {
public:
    ScopeManager();
    void start();
    void stop();
    ScopeSnapshot get_snapshot();
    void set_filter(const std::string &s);
    void clear_filter();
    void export_json(const std::string &path);
    void kill_pid(int pid);
private:
    void loop();
    ProcScopeReader reader;
    ScopeSnapshot snapshot;
    std::mutex mtx;
    bool running=false;
    int refresh_ms = 1000;
    std::string filter;
};
