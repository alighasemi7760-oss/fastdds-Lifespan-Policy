#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>

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
#include <fastdds/dds/core/policy/QosPolicies.hpp>

#include "NetworkMonitor.hpp"

struct DataSampleStruct {
    uint32_t id;
    uint64_t timestamp;
};

class DataSampleStructPubSubType : public eprosima::fastdds::dds::TopicDataType {
public:
    DataSampleStructPubSubType() {
        set_name("DataSampleStruct");
        max_serialized_type_size = sizeof(DataSampleStruct);
        is_compute_key_provided = false;
    }

    bool serialize(const void* data, eprosima::fastdds::rtps::SerializedPayload_t& payload, eprosima::fastdds::dds::DataRepresentationId_t) override {
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

    uint32_t calculate_serialized_size(const void*, eprosima::fastdds::dds::DataRepresentationId_t) override {
        return static_cast<uint32_t>(sizeof(DataSampleStruct));
    }

    void* create_data() override { return new DataSampleStruct(); }
    void delete_data(void* data) override { delete static_cast<DataSampleStruct*>(data); }

    bool compute_key(eprosima::fastdds::rtps::SerializedPayload_t&, eprosima::fastdds::rtps::InstanceHandle_t&, bool) override { return false; }
    bool compute_key(const void*, eprosima::fastdds::rtps::InstanceHandle_t&, bool) override { return false; }
};

using namespace eprosima::fastdds::dds;

// -------------------------------------------------------------
// تابع اعمال پویای QoS (4-Stage Adaptive Policy Engine)
// -------------------------------------------------------------
void apply_adaptive_qos(DataWriter* writer, int stage, int& last_applied_stage) {
    if (stage == last_applied_stage || !writer) return;

    DataWriterQos qos = writer->get_qos();

    switch (stage) {
        case 1: // Stage 1: Optimal Network
            qos.lifespan().duration = Duration_t(2, 0); // 2.0 Seconds Lifespan
            qos.history().depth = 20;                    // Deep History
            qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
            std::cout << "\n[QoS ADAPTER] 🟢 Switched to STAGE 1 (Optimal) -> Lifespan: 2.0s | History: 20 | Reliable\n" << std::endl;
            break;

        case 2: // Stage 2: Mild DDIL
            qos.lifespan().duration = Duration_t(0, 800000000); // 800 ms Lifespan
            qos.history().depth = 10;                        // Medium History
            qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
            std::cout << "\n[QoS ADAPTER] 🟡 Switched to STAGE 2 (Mild DDIL) -> Lifespan: 0.8s | History: 10 | Reliable\n" << std::endl;
            break;

        case 3: // Stage 3: Moderate DDIL
            qos.lifespan().duration = Duration_t(0, 300000000); // 300 ms Lifespan
            qos.history().depth = 3;                         // Shallow History
            qos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
            std::cout << "\n[QoS ADAPTER] 🟠 Switched to STAGE 3 (Moderate DDIL) -> Lifespan: 0.3s | History: 3 | BestEffort\n" << std::endl;
            break;

        case 4: // Stage 4: Severe DDIL / Blackout
            qos.lifespan().duration = Duration_t(0, 100000000); // 100 ms Lifespan
            qos.history().depth = 1;                         // Keep Last 1 Only
            qos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
            std::cout << "\n[QoS ADAPTER] 🔴 Switched to STAGE 4 (Severe DDIL) -> Lifespan: 0.1s | History: 1 | BestEffort\n" << std::endl;
            break;
    }

    writer->set_qos(qos);
    last_applied_stage = stage;
}

class AdaptiveListener : public DataReaderListener {
public:
    explicit AdaptiveListener(NetworkMonitor& monitor) : net_monitor_(monitor) {}

    void on_data_available(DataReader* reader) override {
        SampleInfo info;
        DataSampleStruct sample;
        while (reader->take_next_sample(&sample, &info) == RETCODE_OK) {
            if (!info.valid_data) continue;

            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            int64_t raw_latency = now - sample.timestamp;
            if (raw_latency <= 0) raw_latency = 1;

            net_monitor_.update_latency(raw_latency);

            std::cout << "[Subscriber-Adaptive] Received ID: " << sample.id
                      << " | Latency: " << raw_latency << " ms"
                      << " | Active Stage: " << net_monitor_.get_current_stage()
                      << std::endl;
        }
    }

private:
    NetworkMonitor& net_monitor_;
};

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    std::string role = argv[1];

    DomainParticipant* participant = DomainParticipantFactory::get_instance()->create_participant(0, PARTICIPANT_QOS_DEFAULT);
    if (!participant) return 1;

    TypeSupport type(new DataSampleStructPubSubType());
    type.register_type(participant);

    std::string topic_name = "DataSampleTopic";

    NetworkMonitor net_monitor;
    net_monitor.start();

    if (role == "subscriber") {
        Subscriber* subscriber = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        Topic* topic = participant->create_topic(topic_name, type.get_type_name(), TOPIC_QOS_DEFAULT);

        AdaptiveListener listener(net_monitor);
        DataReaderQos reader_qos = DATAREADER_QOS_DEFAULT;
        
        reader_qos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
        reader_qos.history().kind = KEEP_LAST_HISTORY_QOS;
        reader_qos.history().depth = 20;

        subscriber->create_datareader(topic, reader_qos, &listener);
        while (true) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }

    } else if (role == "publisher") {
        Publisher* publisher = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
        Topic* topic = participant->create_topic(topic_name, type.get_type_name(), TOPIC_QOS_DEFAULT);

        DataWriterQos writer_qos = DATAWRITER_QOS_DEFAULT;
        writer_qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        writer_qos.history().kind = KEEP_LAST_HISTORY_QOS;
        writer_qos.history().depth = 20;

        DataWriter* writer = publisher->create_datawriter(topic, writer_qos);

        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        std::cout << "[Publisher-Adaptive] Transmission started with 4-Stage Lifespan/History Engine..." << std::endl;

        int last_applied_stage = 0;

        for (uint32_t id = 1; id <= 300; ++id) {
            int current_stage = net_monitor.get_current_stage();
            apply_adaptive_qos(writer, current_stage, last_applied_stage);

            DataSampleStruct sample;
            sample.id = id;
            sample.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            writer->write(&sample);

            int sleep_time = (current_stage >= 3) ? 150 : 50; 
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        }
    }

    net_monitor.stop();
    return 0;
}
