#ifndef NETWORK_MONITOR_HPP
#define NETWORK_MONITOR_HPP

#include <atomic>
#include <thread>
#include <cmath>

class NetworkMonitor {
public:
    NetworkMonitor();
    ~NetworkMonitor();

    void start();
    void stop();

    int get_current_stage() const;
    int get_current_stress_index() const;

    // متد جدید برای ثبت سه معیار کلیدی
    void update_metrics(int latency_ms, float loss_rate, int jitter_ms);

    // پشتیبانی کامل از توابع قبلی جهت جلوگیری از خطای کامپایل
    void update_observed_latency(int latency_ms);
    void update_latency(int latency_ms) { update_observed_latency(latency_ms); }

private:
    void monitor_loop();
    int calculate_network_stress();

    std::atomic<int> current_stage_{1};
    std::atomic<int> stress_index_{0};

    std::atomic<int> last_observed_latency_{0};
    std::atomic<float> last_observed_loss_{0.0f};
    std::atomic<int> last_observed_jitter_{0};

    // متغیرهای محاسبه Jitter در لحظه
    int previous_latency_{0};

    std::atomic<bool> running_{false};
    std::thread monitor_thread_;
};

#endif // NETWORK_MONITOR_HPP
