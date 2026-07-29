#include "NetworkMonitor.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

NetworkMonitor::NetworkMonitor() 
    : current_stage_(1), stress_index_(0), last_observed_latency_(0), 
      last_observed_loss_(0.0f), last_observed_jitter_(0), running_(false) {}

NetworkMonitor::~NetworkMonitor() {
    stop();
}

void NetworkMonitor::start() {
    running_ = true;
    monitor_thread_ = std::thread(&NetworkMonitor::monitor_loop, this);
}

void NetworkMonitor::stop() {
    if (running_) {
        running_ = false;
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    }
}

int NetworkMonitor::get_current_stage() const {
    return current_stage_.load();
}

int NetworkMonitor::get_current_stress_index() const {
    return stress_index_.load();
}

void NetworkMonitor::update_metrics(int latency_ms, float loss_rate, int jitter_ms) {
    last_observed_latency_.store(latency_ms);
    last_observed_loss_.store(loss_rate);
    last_observed_jitter_.store(jitter_ms);
}

void NetworkMonitor::update_observed_latency(int latency_ms) {
    int current_lat = latency_ms;
    int calc_jitter = std::abs(current_lat - previous_latency_);
    previous_latency_ = current_lat;

    // تخمین هوشمند افت پکت فرضی بر اساس جهش تاخیر در صورت عدم ورود مستقیم
    float estimated_loss = 0.0f;
    if (current_lat > 800) estimated_loss = 60.0f;
    else if (current_lat > 400) estimated_loss = 30.0f;
    else if (current_lat > 150) estimated_loss = 10.0f;

    update_metrics(current_lat, estimated_loss, calc_jitter);
}

int NetworkMonitor::calculate_network_stress() {
    int lat = last_observed_latency_.load();
    float loss = last_observed_loss_.load();
    int jit = last_observed_jitter_.load();

    // ۱. نرمال‌سازی تاخیر (آستانه بحرانی 1000ms)
    float lat_score = std::min(100.0f, (static_cast<float>(lat) / 1000.0f) * 100.0f);
    
    // ۲. نرمال‌سازی افت پکت (بین 0 تا 100 درصد)
    float loss_score = std::min(100.0f, std::max(0.0f, loss));

    // ۳. نرمال‌سازی جیتر (آستانه بحرانی 200ms)
    float jit_score = std::min(100.0f, (static_cast<float>(jit) / 200.0f) * 100.0f);

    // فرمول ترکیبی NSI با ضریب وزن‌های استاندارد تاکتیکی
    float total_stress = (0.45f * lat_score) + (0.40f * loss_score) + (0.15f * jit_score);
    return static_cast<int>(total_stress);
}

void NetworkMonitor::monitor_loop() {
    int candidate_stage = 1;
    int stability_counter = 0;

    while (running_) {
        int calculated_stress = calculate_network_stress();
        stress_index_.store(calculated_stress);

        int target_stage = 1;
        if (calculated_stress >= 80) {
            target_stage = 6; // Extreme Blackout
        } else if (calculated_stress >= 60) {
            target_stage = 5; // Severe
        } else if (calculated_stress >= 40) {
            target_stage = 4; // High
        } else if (calculated_stress >= 25) {
            target_stage = 3; // Moderate
        } else if (calculated_stress >= 12) {
            target_stage = 2; // Mild
        } else {
            target_stage = 1; // Optimal
        }

        // فیلتر Hysteresis برای جلوگیری از نوسان‌های سریع
        if (target_stage != current_stage_.load()) {
            if (target_stage == candidate_stage) {
                stability_counter++;
            } else {
                candidate_stage = target_stage;
                stability_counter = 1;
            }

            // تایید تغییر استیج پس از ۲ دوره پایش متوالی (۶۰۰ میلی‌ثانیه ثبات)
            if (stability_counter >= 2) {
                current_stage_.store(target_stage);
                std::cout << "[Autonomous QoS Engine] Dynamic Transition -> Stage " << target_stage
                          << " | Network Stress Index (NSI): " << calculated_stress << "/100"
                          << " (Lat: " << last_observed_latency_.load() << "ms, Loss: " 
                          << last_observed_loss_.load() << "%, Jit: " << last_observed_jitter_.load() << "ms)" 
                          << std::endl;
                stability_counter = 0;
            }
        } else {
            stability_counter = 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}
