#include "war_wfde.h"
#include "WfdeServer.h"
#include "WfdeProtocol.h"
#include "tasks/WarThreadpool.h"
#include "log/WarLog.h"

#include "war_helper.h"

using namespace std;

#define LOCK lock_guard<mutex> lock__(mutex_);

std::ostream& operator << (std::ostream& o, const war::wfde::Protocol& entity) {
    return o << "{Protocol " << war::log::Esc(entity.GetName()) << "}";
}


namespace war {
namespace wfde {
namespace impl {

/* Simple repository for protocol factories */
class ProtocolFactory
{
public:
    ProtocolFactory() {}

    // Singleton object
    static ProtocolFactory& GetInstance() {
        static ProtocolFactory instance;
        return instance;
    }

    void Add(const std::string& name, protocol_factory_t&& factory) {
        LOG_DEBUG_FN << "Adding protocol factory for " << log::Esc(name);
        WarMapAddUnique(factories_, name, move(factory));
    }

    const protocol_factory_t& Get(const std::string& name) {
        auto rval = factories_.find(name);
        if (rval == factories_.end()) {
            WAR_THROW_T(ExceptionNotFound, name);
        }
        return rval->second;
    }

private:
    std::map<std::string, protocol_factory_t> factories_;
};



WfdeProtocol::WfdeProtocol(Host *parent, const Configuration::ptr_t& conf)
: WfdeEntity(parent, conf, Type::PROTOCOL)
{
    LOG_DEBUG_FN << "Created host: " << log::Esc(name_);
}
WfdeProtocol::~WfdeProtocol()
{
    LOG_DEBUG_FN << "Deleted host: " << log::Esc(name_);
}

unsigned WfdeProtocol::AddInterfaces()
{
    unsigned added_if_cnt{0};
    const string root{ "/Interfaces" };
    const auto interfaces = conf_->EnumNodes(root.c_str());

    for(const auto name : interfaces) {
        auto path = root + "/" + name.name;

        LOG_TRACE1_FN << "Adding interface on path: " << log::Esc(path);

        auto if_conf = conf_->GetConfigForPath(path);
        added_if_cnt += AddInterface(if_conf);
    }

    return added_if_cnt;
}

unsigned WfdeProtocol::AddInterface(const Configuration::ptr_t& conf)
{
    unsigned added_if_cnt{0};

    const auto if_name = conf->GetValue("/Name");
    const auto if_ip = conf->GetValue("/Ip");
    const auto if_port = conf->GetValue("/Port");

    LOG_DEBUG_FN << "Preparing to add interface(s) with base-name "
        << log::Esc(if_name) << " "
        << log::Esc(if_ip) << " port "
        << log::Esc(if_port)
        << " to " << *this;

    boost::asio::io_service io; // for resolving, no async operations.
    boost::asio::ip::tcp::resolver resolver(io);
    boost::asio::ip::tcp::resolver::iterator end, endpoints;

    try {
        endpoints = resolver.resolve({if_ip, if_port});
    } catch(const boost::exception& ex) {
        LOG_ERROR_FN << "Failed to resolve " << log::Esc(if_name)
        << ": " << ex;
        throw;
    }
    for(;endpoints != end; ++endpoints) {
        auto ep = endpoints->endpoint();
        Interface::ptr_t new_if = CreateInterface(
            this,
            if_name + "-" + ep.address().to_string(),
            endpoints->endpoint(), conf);

        ++added_if_cnt;
        interfaces_.push_back(new_if);

        LOG_DEBUG_FN << "Added interface " << *new_if << " to " << *this;
    }

    return added_if_cnt;
}


void WfdeProtocol::Start()
{
    running_ = true;

    LOG_NOTICE << "Starting " << *this;

    for(auto& interface: GetInterfaces()) {
        auto& pipeline = GetIoThreadpool().GetPipeline(0);

        interface->Start(pipeline, bind(&WfdeProtocol::OnConnected, this,
                                        placeholders::_1));
    }
}

void WfdeProtocol::Stop()
{
    running_ = false;

    LOG_NOTICE << "Stopping " << *this;

    for(auto& interface: GetInterfaces()) {
        interface->Stop();
    }
}

void WfdeProtocol::OnConnected(const Socket::ptr_t& socket)
{
    LOG_NOTICE << "Incoming connection: " << *socket << " on "
        << *GetParent() << " " << *this;

    if (running_) {
        try {
            HandleConnection(socket);
        } WAR_CATCH_ERROR;
    } else {
        LOG_DEBUG_FN << "Dismissing incoming connection as I'm not active";
    }
}

} // namespace impl

Protocol::ptr_t CreateProtocol(Host *parent, const Configuration::ptr_t conf)
{
    auto factory = impl::ProtocolFactory::GetInstance().Get(conf->GetValue("/Name"));
    auto prot = factory(parent, conf);
    if (parent) {
        parent->AddProtocol(prot);
    }
    return prot;
}

void RegisterProtocolFactory(const std::string& name,
                             protocol_factory_t&& factory)
{
    impl::ProtocolFactory::GetInstance().Add(name, move(factory));
}

} // namespace wfde
} // namespace war
