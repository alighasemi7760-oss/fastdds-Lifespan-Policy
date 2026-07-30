#ifndef DATASAMPLEAPPLICATION_HPP
#define DATASAMPLEAPPLICATION_HPP

#include <memory>
#include <string>

class DataSampleApplication
{
public:
    DataSampleApplication(const int& domain_id = 0) : domain_id_(domain_id) {}
    virtual ~DataSampleApplication() = default;

    virtual bool init() = 0;
    virtual void run() = 0;

    static std::shared_ptr<DataSampleApplication> make_app(const int& domain_id, const std::string& role);

protected:
    int domain_id_{0};
};

#endif // DATASAMPLEAPPLICATION_HPP
