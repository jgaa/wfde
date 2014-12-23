#pragma once

#include "wfde/wfde.h"
#include "WfdeEntity.h"

namespace war {
namespace wfde {
namespace impl {

class WfdeServer : public Server,
    public WfdeEntity<Entity, WfdeServer, Host>
{
    using hostslist_t = std::map<std::string, Host::ptr_t>;
public:
    using ptr_t = std::shared_ptr<WfdeServer>;
    using wptr_t = std::weak_ptr<WfdeServer>;

    WfdeServer(Threadpool& ioThreapool, Configuration::ptr_t& conf);
    ~WfdeServer();

    void Start() override;
    void Stop() override;

    children_t GetChildren(const std::string& filter = ".*") const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return GetMyChildren(filter, hosts_);
    }

    hosts_t GetHosts(const std::string& filter = ".*") const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return GetMyChildren<hosts_t>(filter, hosts_);
    }

    Threadpool& GetIoThreadpool() const override { return io_threadpool_; }
    void AddHost(Host::ptr_t host) override;

private:

    hostslist_t hosts_;
    mutable std::mutex mutex_;
    Threadpool& io_threadpool_;
};

} // namespace impl
} // namespace wfde
} // namespace war
