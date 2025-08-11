#include "ui.h"
#include <ncurses.h>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <ctime>

static std::string human_readable_uptime(unsigned long long s) {
    unsigned long long days = s / 86400ULL;
    s %= 86400ULL;
    unsigned long long hours = s / 3600ULL;
    s %= 3600ULL;
    unsigned long long mins = s / 60ULL;
    unsigned long long secs = s % 60ULL;
    std::ostringstream oss;
    if (days) oss<<days<<"d ";
    if (hours) oss<<hours<<"h ";
    oss<<mins<<"m "<<secs<<"s";
    return oss.str();
}

static void draw(const ScopeSnapshot &snap) {
    int rows, cols; getmaxyx(stdscr, rows, cols);
    erase();
    // header
    std::ostringstream hdr;
    hdr<<"ProcScope — Host:"<<snap.hostname<<"  Uptime:"<<human_readable_uptime(snap.uptime_seconds);
    mvprintw(0,0, hdr.str().c_str());
    // second line with CPU/MEM/temp
    std::ostringstream line2; line2<<"CPU:"<<std::fixed<<std::setprecision(1)<<snap.cpu_percent<<"% ";
    line2<<"MEM:"<<std::fixed<<std::setprecision(1)<<snap.mem_percent<<"% ";
    if (snap.cpu_temp_c>-500) line2<<"| Temp:"<<std::fixed<<std::setprecision(1)<<snap.cpu_temp_c<<"C";
    mvprintw(1,0, line2.str().c_str());
    // columns
    mvprintw(3,0, "PID    CPU%%   MEM%%   COMMAND");
    int y=4; int max_show = rows - y - 2; int i=0;
    for (auto &p : snap.procs) {
        if (i++ >= max_show) break;
        mvprintw(y,0, "%-6d %-6.2f %-6.2f %.60s", p.pid, p.cpu_percent, p.mem_percent, p.cmd.c_str());
        y++;
    }
    mvprintw(rows-1,0, "q:quit  /:filter  c:clear  e:export  k:kill");
    refresh();
}

void run_ui(ScopeManager &mgr) {
    initscr(); cbreak(); noecho(); nodelay(stdscr, TRUE); keypad(stdscr, TRUE); curs_set(0);
    bool running=true;
    while (running) {
        ScopeSnapshot snap = mgr.get_snapshot();
        draw(snap);
        int ch = getch();
        if (ch== 'q') { running=false; break; }
        else if (ch == '/') {
            echo(); nodelay(stdscr, FALSE); curs_set(1);
            mvprintw(2,0, "Filter: "); char buf[128]={0}; getnstr(buf,120);
            mgr.set_filter(std::string(buf)); noecho(); nodelay(stdscr, TRUE); curs_set(0);
        } else if (ch=='c') { mgr.clear_filter(); }
        else if (ch=='e') { mgr.export_json("procscope_snapshot.json"); }
        else if (ch=='k') {
            echo(); nodelay(stdscr, FALSE); curs_set(1);
            mvprintw(2,0, "Kill PID: "); char buf[32]={0}; getnstr(buf,30);
            int pid = atoi(buf); mgr.kill_pid(pid);
            noecho(); nodelay(stdscr, TRUE); curs_set(0);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    endwin();
}
