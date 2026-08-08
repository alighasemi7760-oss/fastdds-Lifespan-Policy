#include "NetworkMonitor.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>
#include <vector>
#include <numeric>
#include <mutex>
#include <algorithm>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/topic/TopicDataType.hpp>
#include <fastdds/dds/core/Time_t.hpp>

using namespace eprosima::fastdds::dds;

// ------------------- ساختار داده اصلی (پکت واقعی) -------------------
struct DataSampleStruct {
    uint32_t id;
    uint64_t timestamp;
};

class DataSampleStructPubSubType : public TopicDataType {
public:
    DataSampleStructPubSubType() {
        set_name("DataSampleStructAdaptive");
        max_serialized_type_size = sizeof(DataSampleStruct);
        is_compute_key_provided = false;
    }
    bool serialize(const void* data, eprosima::fastdds::rtps::SerializedPayload_t& payload, DataRepresentationId_t) override {
        auto p = static_cast<const DataSampleStruct*>(data);
        payload.length = sizeof(DataSampleStruct);
        memcpy(payload.data, p, payload.length);
        return true;
    }
    bool deserialize(eprosima::fastdds::rtps::SerializedPayload_t& payload, void* data) override {
        auto p = static_cast<DataSampleStruct*>(data);
        memcpy(p, payload.data, payload.length);
        return true;
    }
    uint32_t calculate_serialized_size(const void*, DataRepresentationId_t) override {
        return static_cast<uint32_t>(sizeof(DataSampleStruct));
    }
    void* create_data() override { return new DataSampleStruct(); }
    void delete_data(void* data) override { delete static_cast<DataSampleStruct*>(data); }
    bool compute_key(eprosima::fastdds::rtps::SerializedPayload_t&, eprosima::fastdds::rtps::InstanceHandle_t&, bool) override { return false; }
    bool compute_key(const void*, eprosima::fastdds::rtps::InstanceHandle_t&, bool) override { return false; }
};

// ------------------- ساختار بازخورد (Subscriber -> Publisher) -------------------
// Subscriber معیارهای واقعی اندازه‌گیری‌شده (شامل throughput) را برای Publisher
// می‌فرستد تا NetworkMonitor سمت Publisher بتواند Lifespan و Deadline را تنظیم کند.
struct NetworkFeedback {
    int32_t latency_ms;
    float loss_rate;
    int32_t jitter_ms;
    float throughput_kbps;   // بُعد Limited: پهنای‌باند واقعی مشاهده‌شده در پنجره اخیر
};

class NetworkFeedbackPubSubType : public TopicDataType {
public:
    NetworkFeedbackPubSubType() {
        set_name("NetworkFeedback");
        max_serialized_type_size = sizeof(NetworkFeedback);
        is_compute_key_provided = false;
    }
    bool serialize(const void* data, eprosima::fastdds::rtps::SerializedPayload_t& payload, DataRepresentationId_t) override {
        auto p = static_cast<const NetworkFeedback*>(data);
        payload.length = sizeof(NetworkFeedback);
        memcpy(payload.data, p, payload.length);
        return true;
    }
    bool deserialize(eprosima::fastdds::rtps::SerializedPayload_t& payload, void* data) override {
        auto p = static_cast<NetworkFeedback*>(data);
        memcpy(p, payload.data, payload.length);
        return true;
    }
    uint32_t calculate_serialized_size(const void*, DataRepresentationId_t) override {
        return static_cast<uint32_t>(sizeof(NetworkFeedback));
    }
    void* create_data() override { return new NetworkFeedback(); }
    void delete_data(void* data) override { delete static_cast<NetworkFeedback*>(data); }
    bool compute_key(eprosima::fastdds::rtps::SerializedPayload_t&, eprosima::fastdds::rtps::InstanceHandle_t&, bool) override { return false; }
    bool compute_key(const void*, eprosima::fastdds::rtps::InstanceHandle_t&, bool) override { return false; }
};

// نگاشت Stage (1..6) به Lifespan: هرچه شبکه بحرانی‌تر، داده‌ی بایات سریع‌تر دور ریخته می‌شود
static int stage_to_lifespan_ms(int stage) {
    switch (stage) {
        case 1: return 5000; // Optimal
        case 2: return 3000; // Mild
        case 3: return 2000; // Moderate
        case 4: return 1000; // High
        case 5: return 500;  // Severe
        case 6: return 200;  // Extreme Blackout
        default: return 5000;
    }
}

// نگاشت Stage به Deadline: برخلاف Lifespan، جهتش معکوس است.
// Deadline یعنی «حداکثر بازه‌ی قابل‌قبول بین دو به‌روزرسانی». هرچه شبکه بحرانی‌تر،
// این بازه باید سهل‌گیرانه‌تر (بزرگ‌تر) شود؛ وگرنه سیستم مدام نقض Deadline گزارش
// می‌دهد بدون این‌که واقعاً کاری از دستش بربیاید.
static int stage_to_deadline_ms(int stage) {
    switch (stage) {
        case 1: return 200;   // Optimal: انتظار به‌روزرسانی مکرر و تازه
        case 2: return 400;
        case 3: return 600;
        case 4: return 1000;
        case 5: return 1500;
        case 6: return 2500;  // Extreme Blackout: بسیار سهل‌گیرانه
        default: return 200;
    }
}

// =====================================================================
// نقش Publisher: می‌نویسد + به Feedback گوش می‌دهد + Lifespan/Deadline را زنده تنظیم می‌کند
// =====================================================================
class FeedbackListener : public DataReaderListener {
public:
    explicit FeedbackListener(NetworkMonitor& monitor) : monitor_(monitor) {}

    void on_data_available(DataReader* reader) override {
        SampleInfo info;
        NetworkFeedback fb;
        while (reader->take_next_sample(&fb, &info) == RETCODE_OK) {
            if (!info.valid_data) continue;
            monitor_.update_metrics(fb.latency_ms, fb.loss_rate, fb.jitter_ms);
            monitor_.update_throughput(fb.throughput_kbps);
        }
    }
private:
    NetworkMonitor& monitor_;
};

static void run_publisher(DomainParticipant* participant) {
    TypeSupport data_type(new DataSampleStructPubSubType());
    data_type.register_type(participant);
    TypeSupport feedback_type(new NetworkFeedbackPubSubType());
    feedback_type.register_type(participant);

    Publisher* publisher = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
    Topic* data_topic = participant->create_topic("DDIL_Adaptive_Topic", data_type.get_type_name(), TOPIC_QOS_DEFAULT);
    Topic* feedback_topic = participant->create_topic("DDIL_Feedback_Topic", feedback_type.get_type_name(), TOPIC_QOS_DEFAULT);

    // BEST_EFFORT عمداً انتخاب شده: با RELIABLE، خود FastDDS پکت گم‌شده را
    // retransmit می‌کند و loss واقعی که NetworkMonitor باید ببیند ماسک می‌شود.
    DataWriterQos writer_qos = DATAWRITER_QOS_DEFAULT;
    writer_qos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
    writer_qos.history().kind = KEEP_LAST_HISTORY_QOS;
    writer_qos.history().depth = 10;
    writer_qos.lifespan().duration = eprosima::fastdds::dds::Duration_t(5, 0); // شروع با Stage 1
    writer_qos.deadline().period = eprosima::fastdds::dds::Duration_t(0, 200 * 1000000); // 200ms، شروع با Stage 1

    DataWriter* writer = publisher->create_datawriter(data_topic, writer_qos);

    Subscriber* subscriber = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    DataReaderQos feedback_reader_qos = DATAREADER_QOS_DEFAULT;
    feedback_reader_qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    feedback_reader_qos.history().kind = KEEP_LAST_HISTORY_QOS;
    feedback_reader_qos.history().depth = 20;

    NetworkMonitor net_monitor;
    FeedbackListener feedback_listener(net_monitor);
    subscriber->create_datareader(feedback_topic, feedback_reader_qos, &feedback_listener);

    net_monitor.start();

    std::cout << "[Publisher-Adaptive] Transmission started (Best-Effort, Adaptive Lifespan+Deadline)..." << std::endl;

    int applied_stage = 1;
    for (uint32_t id = 1; id <= 300; ++id) {
        // اعمال Lifespan و Deadline جدید در صورت تغییر Stage (خواندن زنده از NetworkMonitor)
        int stage_now = net_monitor.get_current_stage();
        if (stage_now != applied_stage) {
            DataWriterQos qos = writer->get_qos();

            int lifespan_ms = stage_to_lifespan_ms(stage_now);
            qos.lifespan().duration = eprosima::fastdds::dds::Duration_t(
                lifespan_ms / 1000, (lifespan_ms % 1000) * 1000000);

            int deadline_ms = stage_to_deadline_ms(stage_now);
            qos.deadline().period = eprosima::fastdds::dds::Duration_t(
                deadline_ms / 1000, (deadline_ms % 1000) * 1000000);

            writer->set_qos(qos);
            std::cout << "[Autonomous QoS Engine] Stage " << applied_stage << " -> " << stage_now
                      << " | New Lifespan: " << lifespan_ms << "ms"
                      << " | New Deadline: " << deadline_ms << "ms"
                      << " (NSI: " << net_monitor.get_current_stress_index() << "/100)" << std::endl;
            applied_stage = stage_now;
        }

        DataSampleStruct sample;
        sample.id = id;
        sample.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        writer->write(&sample);

        // فاصله 190ms عمداً انتخاب شده: 300 * 190ms = 57000ms،
        // دقیقاً برابر مجموع مدت‌زمان مراحل STAGES در ddil_simulation.py
        // (5*6 + 9 + 8 + 10 = 57s، شامل مرحله جدید Intermittent).
        // اگر جدول STAGES تغییر کرد، این مقدار را هم به‌روز کن.
        std::this_thread::sleep_for(std::chrono::milliseconds(190));
    }

    net_monitor.stop();
}

// =====================================================================
// نقش Subscriber: پکت واقعی می‌گیرد، latency/jitter/loss/throughput واقعی حساب می‌کند
// و آن‌ها را روی کانال Feedback برای Publisher می‌فرستد
// =====================================================================
class AdaptiveDataListener : public DataReaderListener {
public:
    explicit AdaptiveDataListener(DataWriter* feedback_writer) : feedback_writer_(feedback_writer) {}

    void on_data_available(DataReader* reader) override {
        SampleInfo info;
        DataSampleStruct sample;
        while (reader->take_next_sample(&sample, &info) == RETCODE_OK) {
            if (!info.valid_data) continue;

            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            int64_t latency = now - static_cast<int64_t>(sample.timestamp);
            if (latency <= 0) latency = 1;

            std::lock_guard<std::mutex> lock(mutex_);

            // تشخیص افت پکت واقعی از روی جهش در id (نه تخمین از روی تاخیر)
            if (expected_id_ != 0 && sample.id > expected_id_) {
                uint32_t missed = sample.id - expected_id_;
                for (uint32_t i = 0; i < missed; ++i) push_window(0);
            }
            push_window(1);
            expected_id_ = sample.id + 1;

            int jitter = (prev_latency_ == 0) ? 0 : static_cast<int>(std::abs(latency - prev_latency_));
            prev_latency_ = latency;

            int received_in_window = std::accumulate(window_.begin(), window_.end(), 0);
            float loss_rate = window_.empty() ? 0.0f :
                (1.0f - (static_cast<float>(received_in_window) / window_.size())) * 100.0f;

            // پنجره‌ی ۲ ثانیه‌ای برای محاسبه throughput واقعی (بُعد Limited)
            recent_times_.push_back(now);
            while (!recent_times_.empty() && (now - recent_times_.front()) > 2000) {
                recent_times_.erase(recent_times_.begin());
            }
            float throughput_kbps = 0.0f;
            if (recent_times_.size() > 1) {
                int64_t span_ms = recent_times_.back() - recent_times_.front();
                if (span_ms > 0) {
                    double bits = static_cast<double>(recent_times_.size()) * sizeof(DataSampleStruct) * 8.0;
                    throughput_kbps = static_cast<float>(bits / span_ms); // bits/ms == kbit/s
                }
            }

            std::cout << "[Subscriber-Adaptive] Received ID: " << sample.id
                      << " | Latency: " << latency << "ms"
                      << " | Jitter: " << jitter << "ms"
                      << " | LossRate: " << static_cast<int>(loss_rate) << "%"
                      << " | Throughput: " << throughput_kbps << "kbps" << std::endl;

            NetworkFeedback fb;
            fb.latency_ms = static_cast<int32_t>(latency);
            fb.loss_rate = loss_rate;
            fb.jitter_ms = jitter;
            fb.throughput_kbps = throughput_kbps;
            feedback_writer_->write(&fb);
        }
    }

private:
    void push_window(int v) {
        window_.push_back(v);
        if (window_.size() > 20) window_.erase(window_.begin());
    }

    DataWriter* feedback_writer_;
    std::mutex mutex_;
    uint32_t expected_id_ = 0;
    int64_t prev_latency_ = 0;
    std::vector<int> window_;
    std::vector<int64_t> recent_times_;
};

static void run_subscriber(DomainParticipant* participant) {
    TypeSupport data_type(new DataSampleStructPubSubType());
    data_type.register_type(participant);
    TypeSupport feedback_type(new NetworkFeedbackPubSubType());
    feedback_type.register_type(participant);

    Publisher* feedback_publisher = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
    Topic* feedback_topic = participant->create_topic("DDIL_Feedback_Topic", feedback_type.get_type_name(), TOPIC_QOS_DEFAULT);
    DataWriterQos feedback_writer_qos = DATAWRITER_QOS_DEFAULT;
    feedback_writer_qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    feedback_writer_qos.history().kind = KEEP_LAST_HISTORY_QOS;
    feedback_writer_qos.history().depth = 20;
    DataWriter* feedback_writer = feedback_publisher->create_datawriter(feedback_topic, feedback_writer_qos);

    Subscriber* subscriber = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    Topic* data_topic = participant->create_topic("DDIL_Adaptive_Topic", data_type.get_type_name(), TOPIC_QOS_DEFAULT);
    DataReaderQos reader_qos = DATAREADER_QOS_DEFAULT;
    reader_qos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
    reader_qos.history().kind = KEEP_LAST_HISTORY_QOS;
    reader_qos.history().depth = 10;

    AdaptiveDataListener listener(feedback_writer);
    subscriber->create_datareader(data_topic, reader_qos, &listener);

    while (true) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    std::string role = argv[1];

    DomainParticipant* participant = DomainParticipantFactory::get_instance()->create_participant(0, PARTICIPANT_QOS_DEFAULT);
    if (!participant) return 1;

    if (role == "publisher") {
        run_publisher(participant);
    } else if (role == "subscriber") {
        run_subscriber(participant);
    }
    return 0;
}
