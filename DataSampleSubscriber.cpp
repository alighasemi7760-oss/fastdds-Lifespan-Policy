#include "DataSampleSubscriber.h"
#include "DataSamplePubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>

#include <chrono>
#include <iostream>

using namespace eprosima::fastdds::dds;

DataSampleSubscriber::DataSampleSubscriber()
    : participant_(nullptr)
    , subscriber_(nullptr)
    , topic_(nullptr)
    , reader_(nullptr)
    , type_(new DataSamplePubSubType())
{
}

DataSampleSubscriber::~DataSampleSubscriber()
{
    if (reader_ != nullptr) { subscriber_->delete_datareader(reader_); }
    if (subscriber_ != nullptr) { participant_->delete_subscriber(subscriber_); }
    if (topic_ != nullptr) { participant_->delete_topic(topic_); }
    DomainParticipantFactory::get_instance()->delete_participant(participant_);
}

class SubListener : public DataReaderListener
{
public:
    void on_data_available(DataReader* reader) override
    {
        DataSample sample;
        SampleInfo info;
        
        if (reader->take_next_sample(&sample, &info) == ReturnCode_t::RETCODE_OK)
        {
            if (info.valid_data)
            {
                auto now = std::chrono::system_clock::now().time_since_epoch();
                uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
                
                // محاسبه تاخیر کل مسیر (Latency)
                uint64_t latency = current_time - sample.timestamp();

                std::cout << "[Subscriber] Received ID: " << sample.id() 
                          << " | Message: " << sample.message() 
                          << " | Total Latency: " << latency << " ms" << std::endl;
                          
                // ارزیابی ایده مقاله:
                // اگر مکانیزم Lifespan به درستی کار کند، در زمان قطع و وصل لینک (DDIL)
                // نباید هیچ پکتی با تاخیر بیشتر از ۵۰۰۰ میلی‌ثانیه (Lifespan) به اینجا برسد!
                if (latency > 5000) {
                    std::cerr << "[WARNING] Stale/Expired data leaked! Latency exceeded Lifespan policy." << std::endl;
                }
            }
        }
    }
};

bool DataSampleSubscriber::init()
{
    DomainParticipantQos pqos;
    pqos.name("Adaptive_DDS_Subscriber");
    participant = DomainParticipantFactory::get_instance()->create_participant(0, pqos);
    if (participant_ == nullptr) return false;

    type_.register_type(participant_);

    subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS);
    if (subscriber_ == nullptr) return false;

    topic_ = participant_->create_topic("DDIL_Adaptive_Topic", type_.get_type_name(), TOPIC_QOS);
    if (topic_ == nullptr) return false;

    DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
    rqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    rqos.history().kind = KEEP_ALL_HISTORY_QOS;

    static SubListener listener;
    reader_ = subscriber_->create_datareader(topic_, rqos, &listener);
    if (reader_ == nullptr) return false;

    return true;
}

void DataSampleSubscriber::run()
{
    std::cout << "[Subscriber] Waiting for data. Press Enter to stop...\n";
    std::cin.ignore();
}
