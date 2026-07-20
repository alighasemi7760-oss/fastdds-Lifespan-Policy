#include "DataSamplePublisher.h"
#include "DataSamplePubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>

#include <thread>
#include <chrono>
#include <iostream>

using namespace eprosima::fastdds::dds;

DataSamplePublisher::DataSamplePublisher()
    : participant_(nullptr)
    , publisher_(nullptr)
    , topic_(nullptr)
    , writer_(nullptr)
    , type_(new DataSamplePubSubType())
{
}

DataSamplePublisher::~DataSamplePublisher()
{
    if (writer_ != nullptr) { publisher_->delete_datawriter(writer_); }
    if (publisher_ != nullptr) { participant_->delete_publisher(publisher_); }
    if (topic_ != nullptr) { participant_->delete_topic(topic_); }
    DomainParticipantFactory::get_instance()->delete_participant(participant_);
}

bool DataSamplePublisher::init()
{
    // ۱. ساخت baseline پارتیسیپنت
    DomainParticipantQos pqos;
    pqos.name("Adaptive_DDS_Publisher");
    participant_ = DomainParticipantFactory::get_instance()->create_participant(0, pqos);
    if (participant_ == nullptr) return false;

    // ۲. ثبت ساختار داده
    type_.register_type(participant_);

    // ۳. ساخت پابلشر
    publisher_ = participant_->create_publisher(PUBLISHER_QOS);
    if (publisher_ == nullptr) return false;

    // ۴. ساخت تاپیک
    topic_ = participant_->create_topic("DDIL_Adaptive_Topic", type_.get_type_name(), TOPIC_QOS);
    if (topic_ == nullptr) return false;

    // ۵. تعریف پیاده‌سازی Lifespan QoS اولیه (مثلا ۵ ثانیه)
    DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;
    
    // تنظیم مدت زمان Lifespan: پکت‌ها پس از ۵ ثانیه اگر ارسال نشوند، منقضی و حذف می‌شوند
    wqos.lifespan().duration = {5, 0}; 
    
    // سیستم قابلیت اطمینان (Reliable) برای اینکه در زمان قطع لینک (DDIL) پکت‌ها در صف بمانند
    wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    wqos.history().kind = KEEP_ALL_HISTORY_QOS; // نگهداری پکت‌ها در بافر در زمان قطعی

    writer_ = publisher_->create_datawriter(topic_, wqos, &listener_);
    if (writer_ == nullptr) return false;

    return true;
}

void DataSamplePublisher::run(uint32_t samples)
{
    uint32_t samples_sent = 0;
    DataSample sample;
    sample.message("DDIL DDS Packet");

    std::cout << "[Publisher] Starting adaptive data transmission. Lifespan set to 5s.\n";

    while (samples_sent < samples)
    {
        sample.id(samples_sent + 1);
        
        // ثبت زمان دقیق سیستم برای محاسبه تاخیر در مقصد
        auto now = std::chrono::system_clock::now().time_since_epoch();
        sample.timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());

        writer_->write(&sample);
        std::cout << "[Publisher] Sent Sample ID: " << sample.id() << std::endl;

        samples_sent++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // ارسال هر ۱ ثانیه یکبار
    }
}
