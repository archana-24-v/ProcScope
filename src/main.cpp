#include "scope_manager.h"
#include "ui.h"
#include <thread>
#include <chrono>

int main() {
    ScopeManager mgr;
    mgr.start();
    // give initial population
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    run_ui(mgr);
    mgr.stop();
    return 0;
}
