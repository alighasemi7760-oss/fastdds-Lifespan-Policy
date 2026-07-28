#include "NetworkMonitor.hpp"

NetworkMonitor::NetworkMonitor() = default;

NetworkMonitor::~NetworkMonitor() {
    stop();
}

void NetworkMonitor::start() {
    if (!running_.load()) {
        running_.store(true);
        worker_thread_ = std::thread(&NetworkMonitor::monitor_loop, this);
    }
}

void NetworkMonitor::stop() {
    if (running_.load()) {
        running_.store(false);
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
}

void NetworkMonitor::update_latency(int64_t latency_ms) {
    current_latency_.store(latency_ms);
}

bool NetworkMonitor::is_high_congestion() const {
    return high_congestion_.load();
}

int64_t NetworkMonitor::get_avg_latency() const {
    return current_latency_.load();
}

void NetworkMonitor::monitor_loop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        int64_t latency = current_latency_.load();
        
        if (latency > 100) {
            if (!high_congestion_.load()) {
                high_congestion_.store(true);
                std::cout << "\n[NETWORK MONITOR] ⚠️ High Congestion Detected! (Avg Latency: " 
                          << latency << "ms) -> Switching to Aggressive Adaptive Policy.\n" << std::endl;
            }
        } else {
            if (high_congestion_.load()) {
                high_congestion_.store(false);
                std::cout << "\n[NETWORK MONITOR] ✅ Network Recovered. Returning to Normal Policy.\n" << std::endl;
            }
        }
    }
}
