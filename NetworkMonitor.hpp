#ifndef NETWORK_MONITOR_HPP
#define NETWORK_MONITOR_HPP

#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>

class NetworkMonitor {
public:
    NetworkMonitor();
    ~NetworkMonitor();

    void start();
    void stop();

    void update_latency(int64_t latency_ms);
    bool is_high_congestion() const;
    int64_t get_avg_latency() const;

    // متد تعیین استیج پویای شبکه بر اساس تاخیر
    int get_current_stage() const {
        int64_t lat = current_latency_.load();
        if (lat <= 50)  return 1; // Stage 1: Optimal
        if (lat <= 200) return 2; // Stage 2: Mild DDIL
        if (lat <= 500) return 3; // Stage 3: Moderate DDIL
        return 4;                 // Stage 4: Severe DDIL / Blackout
    }

private:
    void monitor_loop();

    std::atomic<int64_t> current_latency_{0};
    std::atomic<bool> high_congestion_{false};
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
};

/*
===============================================================================
                       FILE DOCUMENTATION / SUMMARY
===============================================================================
File Name   : NetworkMonitor.hpp
Role        : Header file for the Background Network Monitoring Subsystem

Description :
This header defines the `NetworkMonitor` class, which runs an asynchronous 
background thread to track real-time network metrics (such as Latency and Congestion).

Key Components & Design:
- Asynchronous Monitoring Thread: `worker_thread_` runs `monitor_loop()` independently.
- Thread-Safe Operations: Uses `std::atomic` variables (`current_latency_`, 
  `high_congestion_`, `running_`) to prevent race conditions during concurrent access.
- Metric Updates: `update_latency()` updates the current latency from incoming DDS frames.
- Multi-Stage Classifier (`get_current_stage`): Maps real-time latency thresholds 
  (50ms, 200ms, 500ms) to a 4-Stage operational mode used for dynamic QoS adaptation.
===============================================================================
*/

#endif // NETWORK_MONITOR_HPP
