#include "DataSampleSubscriberApp.hpp"
#include "DataSamplePubSubTypes.hpp"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/core/ReturnCode.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace eprosima::fastdds::dds;

DataSampleSubscriberApp::DataSampleSubscriberApp(const int& domain_id)
    : DataSampleApplication(domain_id)
    , type_(new DataSamplePubSubType())
{
}

DataSampleSubscriberApp::~DataSampleSubscriberApp()
{
    stop();
    if (reader_ != nullptr) { subscriber_->delete_datareader(reader_); }
    if (subscriber_ != nullptr) { participant_->delete_subscriber(subscriber_); }
    if (topic_ != nullptr) { participant_->delete_topic(topic_); }
    DomainParticipantFactory::get_instance()->delete_participant(participant_);
}

void DataSampleSubscriberApp::SubListener::on_data_available(DataReader* reader)
{
    DataSample sample;
    SampleInfo info;

    if (reader->take_next_sample(&sample, &info) == RETCODE_OK)
    {
        if (info.valid_data)
        {
            auto now = std::chrono::system_clock::now().time_since_epoch();
            uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
            uint64_t latency = current_time - sample.timestamp();

            std::cout << "[Subscriber] Received ID: " << sample.id()
                      << " | Total Latency: " << latency << " ms";

            if (latency > 5000) {
                std::cout << " <-- [WARNING: Stale Data Leaked!]";
            }
            std::cout << std::endl;
        }
    }
}

bool DataSampleSubscriberApp::init()
{
    DomainParticipantQos pqos;
    pqos.name("DDS_Subscriber");
    participant_ = DomainParticipantFactory::get_instance()->create_participant(domain_id_, pqos);
    if (participant_ == nullptr) return false;

    type_.register_type(participant_);

    subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    if (subscriber_ == nullptr) return false;

    topic_ = participant_->create_topic("DDIL_Adaptive_Topic", type_.get_type_name(), TOPIC_QOS_DEFAULT);
    if (topic_ == nullptr) return false;

    DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
    rqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    rqos.history().kind = KEEP_ALL_HISTORY_QOS;

    reader_ = subscriber_->create_datareader(topic_, rqos, &listener_);
    if (reader_ == nullptr) return false;

    return true;
}

void DataSampleSubscriberApp::run()
{
    std::cout << "[Subscriber] Waiting for data...\n";
    while (!is_stopped_)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

void DataSampleSubscriberApp::stop()
{
    is_stopped_ = true;
}
