#include "NetworkMonitor.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>
#include <numeric>

// ساختار شبیه‌سازی نمونه داده
struct DataSample {
    int id;
    long long timestamp;
    char payload[1024];
};

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << " Launching Autonomous Adaptive DDIL QoS Engine    " << std::endl;
    std::cout << "==================================================" << std::endl;

    NetworkMonitor net_monitor;
    net_monitor.start();

    // متغیرهای محاسبه Jitter و Packet Loss
    int prev_latency = 0;
    int expected_seq_id = 1;
    int total_expected = 0;
    int total_received = 0;
    
    // پنجره لغزان برای محاسبه نرخ افت پکت اخیر (10 پکت اخیر)
    std::vector<int> reception_window; 

    // شبیه‌سازی دریافت ۳۰۰ پکت با شرایط متغیر شبکه (DDIL Scenario)
    for (int seq_id = 1; seq_id <= 300; ++seq_id) {
        
        // شبیه‌سازی تاخیر و افت پکت بر اساس سناریوی DDIL
        int simulated_latency = 10; // شرایط نرمال S1
        bool packet_dropped = false;

        if (seq_id > 60 && seq_id <= 145) {
            simulated_latency = 100 + (rand() % 40); // شرایط Mild (S2)
        } else if (seq_id > 145 && seq_id <= 210) {
            simulated_latency = 250 + (rand() % 100); // شرایط Moderate (S3)
            if (rand() % 100 < 15) packet_dropped = true; // 15% افت پکت
        } else if (seq_id > 210 && seq_id <= 250) {
            simulated_latency = 450 + (rand() % 150); // شرایط High (S4)
            if (rand() % 100 < 35) packet_dropped = true; // 35% افت پکت
        } else if (seq_id > 250) {
            simulated_latency = 700 + (rand() % 250); // شرایط Severe (S5)
            if (rand() % 100 < 60) packet_dropped = true; // 60% افت پکت
        }

        total_expected++;

        if (packet_dropped) {
            reception_window.push_back(0); // 0 یعنی پکت افتاده
        } else {
            total_received++;
            reception_window.push_back(1); // 1 یعنی پکت دریافت شده

            // ۱. محاسبه Jitter لحظه‌ای
            int current_jitter = (prev_latency == 0) ? 0 : std::abs(simulated_latency - prev_latency);
            prev_latency = simulated_latency;

            // ۲. محاسبه Packet Loss Rate در پنجره ۲۰ پکت اخیر
            if (reception_window.size() > 20) {
                reception_window.erase(reception_window.begin());
            }

            int received_in_window = std::accumulate(reception_window.begin(), reception_window.end(), 0);
            float current_loss_rate = (1.0f - (static_cast<float>(received_in_window) / reception_window.size())) * 100.0f;

            // ۳. بروزرسانی مانیتور شبکه با داده‌های واقعی ۳‌گانه
            net_monitor.update_metrics(simulated_latency, current_loss_rate, current_jitter);

            int current_stage = net_monitor.get_current_stage();
            int current_nsi = net_monitor.get_current_stress_index();

            std::cout << "[Subscriber Adaptive] Received ID: " << seq_id 
                      << " | Latency: " << simulated_latency << "ms"
                      << " | Jitter: " << current_jitter << "ms"
                      << " | LossRate: " << static_cast<int>(current_loss_rate) << "%"
                      << " | Active Stage: " << current_stage 
                      << " (NSI: " << current_nsi << ")" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    net_monitor.stop();
    std::cout << "\n[✔] Adaptive QoS Processing Finished." << std::endl;
    return 0;
}
