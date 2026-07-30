#ifndef DATASAMPLESUBSCRIBERAPP_HPP
#define DATASAMPLESUBSCRIBERAPP_HPP

#include "DataSampleApplication.hpp"
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include "DataSamplePubSubTypes.hpp"

class DataSampleSubscriberApp : public DataSampleApplication
{
public:
    DataSampleSubscriberApp(const int& domain_id = 0);
    virtual ~DataSampleSubscriberApp();

    bool init() override;
    void run() override;
    void stop();

private:
    eprosima::fastdds::dds::DomainParticipant* participant_{nullptr};
    eprosima::fastdds::dds::Subscriber* subscriber_{nullptr};
    eprosima::fastdds::dds::Topic* topic_{nullptr};
    eprosima::fastdds::dds::DataReader* reader_{nullptr};
    eprosima::fastdds::dds::TypeSupport type_;

    bool is_stopped_{false};

    class SubListener : public eprosima::fastdds::dds::DataReaderListener
    {
    public:
        void on_data_available(eprosima::fastdds::dds::DataReader* reader) override;
    } listener_;
};

#endif // DATASAMPLESUBSCRIBERAPP_HPP
