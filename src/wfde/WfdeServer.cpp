
#include <regex>
#include "war_wfde.h"
#include "war_transaction.h"
#include "war_helper.h"
#include "WfdeServer.h"

using namespace std;

#define LOCK lock_guard<mutex> lock__(mutex_);

std::ostream& operator << (std::ostream& o, const war::wfde::Server& entity) {
    return o << "{Server " << war::log::Esc(entity.GetName()) << "}";
}


namespace war {
namespace wfde {
namespace impl {

WfdeServer::WfdeServer(Threadpool& ioThreapool, Configuration::ptr_t& conf)
: WfdeEntity(nullptr, conf), io_threadpool_{ioThreapool}
{
    //
    LOG_DEBUG << "Server " << log::Esc(name_) << " is constructed.";
}

WfdeServer::~WfdeServer()
{
    LOG_DEBUG << "Server " << log::Esc(name_) << " is disposed.";
}

void WfdeServer::Start()
{
    LOG_NOTICE << "Starting the services for " << *this;

    for(auto& p : GetHosts()) {
        p->Start();
    }

    LOG_NOTICE << "Done starting the services for " << *this;
}

void WfdeServer::Stop()
{
    LOG_NOTICE << "Stopping the services for " << *this;

    for(auto& p : GetHosts()) {
        p->Stop();
    }

    LOG_NOTICE << "Done stopping the services for " << *this;
}

void WfdeServer::AddHost(Host::ptr_t host)
{
    LOG_NOTICE << "Adding " << *host << " to " << *this;

    Transaction trans;

    {
        LOCK;
        WarMapAddUnique(hosts_, host->GetName(), move(host));
    }

    // Prepare rollback if SetParent() throws.
    trans.AddUndo([&] {
        LOCK;
        hosts_.erase(host->GetName());
    });
    host->SetParent(*this);

    trans.Commit();
}

} // namespace impl

Server::ptr_t CreateServer(Configuration::ptr_t conf, Threadpool& ioThreadpool)
{
    return make_shared<impl::WfdeServer>(ioThreadpool, conf);
}


} // namespace wfde
} // namespace war
