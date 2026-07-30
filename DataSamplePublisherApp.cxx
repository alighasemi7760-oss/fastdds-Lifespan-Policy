#include "DataSamplePublisherApp.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>

using namespace eprosima::fastdds::dds;

// اگر کلاس داخل namespace یا فایل متفاوتی است، متغیرهای QoS را مستقیم تنظیم می‌کنیم
