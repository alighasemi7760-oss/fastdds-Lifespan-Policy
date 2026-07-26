#include <iostream>
#include <string>
#include <thread>
#include <chrono>

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

#include "../DataSamplePubSubTypes.hpp"

using namespace eprosima::fastdds::dds;

class CustomDataReaderListener : public DataReaderListener {
private:
    bool is_adaptive;
public:
    CustomDataReaderListener(bool adaptive) : is_adaptive(adaptive) {}

    void on_data_available(DataReader* reader) override {
        SampleInfo info;
        DataSample sample;
        
        while (reader->take_next_sample(&sample, &info) == RETCODE_OK) {
            if (!info.valid_data) continue;

            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            int64_t raw_latency = now - sample.timestamp();
            if (raw_latency <= 0) raw_latency = 1;

            std::cout << "[Subscriber] Received ID: " << sample.id() 
                      << " | Total Latency: " << raw_latency << " ms" << std::endl;
        }
    }
};

int main(int argc, char** argv) {
    if (argc < 2) return 1;

    std::string role = argv[1];
    std::string mode = (argc >= 3) ? argv[2] : "standard";
    bool is_adaptive = (mode == "adaptive");

    DomainParticipant* participant = DomainParticipantFactory::get_instance()->create_participant(0, PARTICIPANT_QOS_DEFAULT);
    if (!participant) return 1;

    // استفاده از TypeSupport تولید شده توسط FastDDS-Gen
    TypeSupport type(new DataSamplePubSubType());
    type.register_type(participant);

    if (role == "subscriber") {
        Subscriber* subscriber = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
        Topic* topic = participant->create_topic("DDIL_Topic", type.get_type_name(), TOPIC_QOS_DEFAULT);
        
        CustomDataReaderListener listener(is_adaptive);
        DataReaderQos reader_qos = DATAREADER_QOS_DEFAULT;
        if (is_adaptive) {
            reader_qos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
        }

        subscriber->create_datareader(topic, reader_qos, &listener);
        while (true) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }

    } else if (role == "publisher") {
        Publisher* publisher = participant->create_publisher(PUBLISHER_QOS_DEFAULT);
        Topic* topic = participant->create_topic("DDIL_Topic", type.get_type_name(), TOPIC_QOS_DEFAULT);
        DataWriterQos writer_qos = DATAWRITER_QOS_DEFAULT;

        if (is_adaptive) {
            std::cout << "[QoS] ADAPTIVE Mode enabled (Best-Effort + Lifespan = 1.5s)" << std::endl;
            writer_qos.reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
            writer_qos.lifespan().duration = {1, 500000000};
            writer_qos.history().kind = KEEP_LAST_HISTORY_QOS;
            writer_qos.history().depth = 1;
        } else {
            std::cout << "[QoS] STANDARD Mode enabled (Reliable + Keep All)" << std::endl;
            writer_qos.reliability().kind = RELIABLE_RELIABILITY_QOS;
            writer_qos.history().kind = KEEP_ALL_HISTORY_QOS;
        }

        DataWriter* writer = publisher->create_datawriter(topic, writer_qos);
        std::cout << "[Publisher] Transmission started..." << std::endl;

        for (uint32_t id = 1; id <= 300; ++id) {
            DataSample sample;
            sample.id(id);
            sample.timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

            writer->write(&sample);
            std::cout << "[Publisher] Sent Sample ID: " << id << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    return 0;
}
