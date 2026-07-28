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

class StandardListener : public DataReaderListener {
public:
    void on_data_available(DataReader* reader) override {
        SampleInfo info;
        DataSampleStruct sample;
        while (reader->take_next_sample(&sample, &info) == RETCODE_OK) {
            if (!info.valid_data) continue;

            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            int64_t raw_latency = now - sample.timestamp;
            if (raw_latency <= 0) raw_latency = 1;

            std::cout << "[Subscriber-Standard] Received ID: " << sample.id
                      << " | Total Latency: " << raw_latency << " ms" << std::endl;
        }
    }
};

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    std::string role = argv[1];

    DomainParticipant* participant = DomainParticipantFactory::get_instance()->create_participant(0, PARTICIPANT_QOS_DEFAULT);
    if (!participant) return 1;

    TypeSupport type(new DataSampleStructPubSubType());
    type.register_type(participant);

    if (role == "subscriber") {
        Subscriber* subscriber = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        Topic* topic = participant->create_topic("DDIL_Standard_Topic", type.get_type_name(), TOPIC_QOS_DEFAULT);

        StandardListener listener;
        DataReaderQos reader_qos = DATAREADER_QOS_DEFAULT;
        reader_qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        reader_qos.history().kind = KEEP_ALL_HISTORY_QOS;

        subscriber->create_datareader(topic, reader_qos, &listener);
        while (true) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }

    } else if (role == "publisher") {
        Publisher* publisher = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
        Topic* topic = participant->create_topic("DDIL_Standard_Topic", type.get_type_name(), TOPIC_QOS_DEFAULT);
        
        DataWriterQos writer_qos = DATAWRITER_QOS_DEFAULT;
        writer_qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
        writer_qos.history().kind = KEEP_ALL_HISTORY_QOS;

        DataWriter* writer = publisher->create_datawriter(topic, writer_qos);
        std::cout << "[Publisher-Standard] Transmission started (Reliable)..." << std::endl;

        for (uint32_t id = 1; id <= 300; ++id) {
            DataSampleStruct sample;
            sample.id = id;
            sample.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            writer->write(&sample);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    return 0;
}
