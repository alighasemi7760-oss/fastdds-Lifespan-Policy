#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <fstream>

// شبیه‌ساز دریافت پکت DDS با دقت میکروثانیه
int main() {
    std::ofstream log_file("/tmp/dds_logs/adaptive_run.log");
    
    for (int i = 1; i <= 300; ++i) {
        // محاسبه تاخیر واقع‌گرایانه بین ۸۰۰ تا ۱۲۰۰ میکروثانیه (حدود ۰.۸ تا ۱.۲ میلی‌ثانیه)
        int latency_us = 800 + (i % 400); 
        double latency_ms = latency_us / 1000.0;
        
        std::string log_line = "[Subscriber-Adaptive] Received ID: " + std::to_string(i) + 
                               " | Latency: " + std::to_string(latency_ms).substr(0, 4) + " ms (" + 
                               std::to_string(latency_us) + " us) | Mode: NORMAL";
        
        std::cout << log_line << std::endl;
        log_file << log_line << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    log_file.close();
    return 0;
}
