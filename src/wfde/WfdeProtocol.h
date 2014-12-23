#pragma once

#include <thread>
#include <atomic>

#include "wfde/wfde.h"
#include "WfdeEntity.h"
#include "WfdeHost.h"

namespace war {
namespace wfde {
namespace impl {

class WfdeProtocol : public Protocol,
    public WfdeEntity<Host, WfdeProtocol, Interface>
{
public:
    using ptr_t = std::shared_ptr<WfdeProtocol>;
    using wptr_t = std::weak_ptr<WfdeProtocol>;

    WfdeProtocol(Host *parent, const Configuration::ptr_t& conf);
    ~WfdeProtocol();

    children_t GetChildren(const std::string& filter) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return GetMyChildren(filter, interfaces_);
    }

    interfaces_t GetInterfaces(const std::string& filter = ".*") const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return GetMyChildren<interfaces_t>(filter, interfaces_);
    }

    Host& GetHost() override {
        auto host = GetParent();
        WAR_ASSERT(host != nullptr);
        return *host;
    }

    unsigned AddInterfaces() override;

    void Start() override;
    void Stop() override;

protected:
    // Entry-point for protocol implementations to deal with an incoming connection
    virtual void HandleConnection(const Socket::ptr_t& socket) = 0;

private:
    void OnConnected(const Socket::ptr_t& socket);

    interfaces_t interfaces_;
    mutable std::mutex mutex_; // For simple in-function resource locks only
    std::atomic_bool running_;
};

} // namespace impl
} // namespace wfde
} // namespace war
