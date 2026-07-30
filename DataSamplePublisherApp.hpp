#ifndef DATASAMPLEPUBLISHERAPP_HPP
#define DATASAMPLEPUBLISHERAPP_HPP

#include "DataSampleApplication.hpp"
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include "DataSamplePubSubTypes.hpp"

class DataSamplePublisherApp : public DataSampleApplication
{
public:
    DataSamplePublisherApp(const int& domain_id = 0);
    virtual ~DataSamplePublisherApp();

    bool init() override;
    bool init(bool enable_lifespan);
    void run() override;
    void run(uint32_t samples);
    void stop();

private:
    eprosima::fastdds::dds::DomainParticipant* participant_{nullptr};
    eprosima::fastdds::dds::Publisher* publisher_{nullptr};
    eprosima::fastdds::dds::Topic* topic_{nullptr};
    eprosima::fastdds::dds::DataWriter* writer_{nullptr};
    eprosima::fastdds::dds::TypeSupport type_;

    bool is_stopped_{false};

    class PubListener : public eprosima::fastdds::dds::DataWriterListener
    {
    } listener_;
};

#endif // DATASAMPLEPUBLISHERAPP_HPP
