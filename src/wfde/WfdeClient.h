#pragma once

#include <atomic>
#include <wfde/wfde.h>
#include <warlib/WarPipeline.h>

namespace war {
namespace wfde {
namespace impl {


class WfdeClient : public Client
{
public:
    WfdeClient(std::string loginName, const boost::uuids::uuid& uuid);
    ~WfdeClient();

    const std::string& GetLoginName() const override {
        return login_name_;
    }

    const boost::uuids::uuid& GetUuid() const override {
        return uuid_;
    }

    int GetNumInstances() const override { return instances_; }

private:
    const std::string login_name_;
    const boost::uuids::uuid uuid_;
    std::atomic_int instances_; // Instances of the Client in use in sessions
};



} // namespace impl
} // namespace wfde
} // namespace war
