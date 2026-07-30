#include "DataSampleApplication.hpp"
#include "DataSamplePublisherApp.hpp"
#include "DataSampleSubscriberApp.hpp"

std::shared_ptr<DataSampleApplication> DataSampleApplication::make_app(const int& domain_id, const std::string& role)
{
    std::shared_ptr<DataSampleApplication> entity = nullptr;

    if (role == "publisher" || role == "pub")
    {
        entity = std::make_shared<DataSamplePublisherApp>(domain_id);
    }
    else if (role == "subscriber" || role == "sub")
    {
        entity = std::make_shared<DataSampleSubscriberApp>(domain_id);
    }

    return entity;
}
