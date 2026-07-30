#include "DataSamplePublisherApp.hpp"
#include "DataSampleSubscriberApp.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    std::string type = "publisher";

    if (argc > 1)
    {
        type = argv[1];
    }

    if (type == "publisher" || type == "pub")
    {
        DataSamplePublisherApp publisher;
        if (publisher.init())
        {
            publisher.run();
        }
    }
    else if (type == "subscriber" || type == "sub")
    {
        DataSampleSubscriberApp subscriber;
        if (subscriber.init())
        {
            subscriber.run();
        }
    }
    else
    {
        std::cout << "Usage: " << argv[0] << " [publisher|subscriber]" << std::endl;
        return 1;
    }

    return 0;
}
