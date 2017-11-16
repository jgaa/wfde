#include "war_wfde.h"
#include "WfdeServer.h"
#include "WfdeHost.h"
#include <warlib/WarLog.h>

using namespace std;

#define LOCK lock_guard<mutex> lock__(mutex_);

std::ostream& operator << (std::ostream& o, const war::wfde::Host& entity) {
    return o << "{Host " << war::log::Esc(entity.GetName()) << "}";
}

namespace war {
namespace wfde {
namespace impl {

WfdeHost::WfdeHost(Server& parent, AuthManager::ptr_t& authManager,
                   const Configuration::ptr_t& conf)
    : WfdeEntity(nullptr, conf, Type::HOST)
    , long_name_{conf->GetValue("/LongName"
    , WFDE_DEFAULT_HOST_LONG_NAME)}
    , session_manager_{SessionManager::Create(parent.GetIoThreadpool())}
    , auth_manager_{authManager}
{
    LOG_DEBUG_FN << "Created host: " << log::Esc(name_);
}

WfdeHost::~WfdeHost()
{
    LOG_DEBUG_FN << "Deleted host: " << log::Esc(name_);
}

std::string WfdeHost::GetLongName() const
{
    return long_name_;
}

void WfdeHost::AddProtocol(const Protocol::ptr_t& protocol)
{
    LOG_NOTICE << "Adding " << *protocol << " to " << *this;

    LOCK;
    protocols_.push_back(protocol);
}

void WfdeHost::Start()
{
    LOG_NOTICE << "Starting " << *this;
    for(auto& p : GetProtocols()) {
        p->Start();
    }
}

void WfdeHost::Stop()
{
    LOG_NOTICE << "Stopping " << *this;
    for(auto& p : GetProtocols()) {
        p->Stop();
    }
}


} // namespace impl

Host::ptr_t CreateHost(Server& parent, AuthManager::ptr_t authManager,
                       const Configuration::ptr_t& conf)
{
    auto host = make_shared<impl::WfdeHost>(parent, authManager, conf);
    parent.AddHost(host);
    authManager->Join(host);
    return host;
}

} // namespace wfde
} // namespace war
