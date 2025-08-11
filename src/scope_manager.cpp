#include "scope_manager.h"
#include <thread>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <signal.h>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ScopeManager::ScopeManager() {
    snapshot.hostname = reader.read_hostname();
    snapshot.uptime_seconds = reader.read_uptime_seconds();
    snapshot.cpu_temp_c = -1000.0f;
}

void ScopeManager::start() {
    running = true;
    std::thread([this]{ loop(); }).detach();
}

void ScopeManager::stop() {
    running = false;
}

void ScopeManager::loop() {
    CpuTotals prev = reader.read_cpu_totals();
    static std::map<int, unsigned long long> last_proc_ticks;
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(refresh_ms));
        CpuTotals now = reader.read_cpu_totals();
        unsigned long long prev_total = prev.total();
        unsigned long long now_total = now.total();
        unsigned long long total_diff = now_total > prev_total ? now_total - prev_total : 0;
        unsigned long long idle_diff = now.idle > prev.idle ? now.idle - prev.idle : 0;
        float cpu_percent = 0.0f;
        if (total_diff>0) cpu_percent = (float)(total_diff - idle_diff) * 100.0f / (float)total_diff;
        auto procs = reader.read_processes();
        for (auto &p : procs) {
            unsigned long long prev_ticks = last_proc_ticks[p.pid];
            unsigned long long now_ticks = p.total_ticks;
            unsigned long long diff = now_ticks >= prev_ticks ? now_ticks - prev_ticks : 0;
            last_proc_ticks[p.pid] = now_ticks;
            float p_cpu = 0.0f;
            if (total_diff>0) p_cpu = (float)diff * 100.0f / (float)total_diff;
            p.cpu_percent = p_cpu;
        }
        if (!filter.empty()) {
            std::vector<ProcInfo> filtered;
            for (auto &p: procs) if (p.cmd.find(filter)!=std::string::npos) filtered.push_back(p);
            procs.swap(filtered);
        }
        std::sort(procs.begin(), procs.end(), [](const ProcInfo &a, const ProcInfo &b){ return a.cpu_percent > b.cpu_percent; });
        ScopeSnapshot snap;
        snap.cpu = now;
        snap.cpu_percent = cpu_percent;
        snap.mem_percent = reader.read_memory_percent();
        snap.procs = procs;
        snap.uptime_seconds = reader.read_uptime_seconds();
        snap.hostname = reader.read_hostname();
        float temp = 0.0f;
        if (reader.read_cpu_temperature(temp)) snap.cpu_temp_c = temp; else snap.cpu_temp_c = -1000.0f;
        {
            std::lock_guard<std::mutex> lock(mtx);
            snapshot = snap;
        }
        prev = now;
    }
}

ScopeSnapshot ScopeManager::get_snapshot() {
    std::lock_guard<std::mutex> lock(mtx);
    return snapshot;
}

void ScopeManager::set_filter(const std::string &s) {
    std::lock_guard<std::mutex> lock(mtx);
    filter = s;
}

void ScopeManager::clear_filter() {
    std::lock_guard<std::mutex> lock(mtx);
    filter.clear();
}

void ScopeManager::export_json(const std::string &path) {
    ScopeSnapshot snap = get_snapshot();
    json j;
    j["hostname"] = snap.hostname;
    j["uptime_seconds"] = snap.uptime_seconds;
    j["cpu_percent"] = snap.cpu_percent;
    j["mem_percent"] = snap.mem_percent;
    if (snap.cpu_temp_c > -500) j["cpu_temp_c"] = snap.cpu_temp_c;
    j["processes"] = json::array();
    for (auto &p : snap.procs) {
        json pj; pj["pid"] = p.pid; pj["cmd"] = p.cmd; pj["cpu_percent"] = p.cpu_percent; pj["mem_percent"] = p.mem_percent;
        j["processes"].push_back(pj);
    }
    std::ofstream of(path); of<<j.dump(2); of.close();
}

void ScopeManager::kill_pid(int pid) {
    if (pid<=0) return;
    kill(pid, SIGTERM);
}
