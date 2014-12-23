#pragma once

#include <thread>
#include "wfde/wfde.h"
#include "WfdeEntity.h"

namespace war {
namespace wfde {
namespace impl {

class WfdeHost : public Host,
public WfdeEntity<Server, WfdeHost, Protocol>
{
public:
    using ptr_t = std::shared_ptr<WfdeHost>;
    using wptr_t = std::weak_ptr<WfdeHost>;

    WfdeHost(Server& parent, AuthManager& authManager,
             const Configuration::ptr_t& conf);
    ~WfdeHost();

    std::string GetLongName() const override;

    void AddProtocol(const Protocol::ptr_t& protocol) override;

    children_t GetChildren(const std::string& filter = ".*") const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return GetMyChildren(filter, protocols_);
    }

    protocols_t GetProtocols(const std::string& filter = ".*") const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return GetMyChildren<protocols_t>(filter, protocols_);
    }

    Server::ptr_t GetServer() {
        return GetParent()->shared_from_this();
    }

    SessionManager& GetSessionManager() override { return *session_manager_;}

    void Start() override;
    void Stop() override;

    virtual AuthManager& GetAuthManager() { return *auth_manager_; }

private:
    const std::string long_name_;
    protocols_t protocols_;
    mutable std::mutex mutex_; // For simple in-function resource locks only
    Server::wptr_t parent_;
    SessionManager::ptr_t session_manager_;
    AuthManager::ptr_t auth_manager_;
};

} // namespace impl
} // namespace wfde
} // namespace war
