#include "war_wfde.h"

#include <boost/asio/spawn.hpp>

#include "tasks/WarThreadpool.h"
#include "WfdeServer.h"
#include "WfdeInterface.h"
#include "WfdeTlsSocket.h"
#include "WfdeSocket.h"
#include "log/WarLog.h"

using namespace std;

std::ostream& operator << (std::ostream& o, const war::wfde::Interface& entity) {
    return o << "{Interface " << war::log::Esc(entity.GetName()) << "}";
}

std::ostream& operator << (std::ostream& o, const war::wfde::Socket& sck) {
    o << "{Socket " << war::log::Esc(sck.GetName()) << " ("
        << sck.GetSocketVal()
        << ")";

    if (sck.IsOpen()) {
        o << ' ' << sck.GetSocket().local_endpoint()
            << " <--> "
            << sck.GetSocket().remote_endpoint();
    }

    return o << '}';
}

#define LOCK lock_guard<mutex> lock__(mutex_);

namespace war {
namespace wfde {
namespace impl {

WfdeInterface::WfdeInterface(Protocol *parent,
                             const boost::asio::ip::tcp::endpoint& endpoint,
                             Configuration::ptr_t& conf)
: WfdeEntity(parent, conf), endpoint_{endpoint}
{
    LOG_DEBUG_FN << "Created interface: " << log::Esc(name_);
}

WfdeInterface::~WfdeInterface()
{
    LOG_DEBUG_FN << "Deleted interface: " << log::Esc(name_);
}


boost::asio::ip::tcp::endpoint WfdeInterface::GetEndpoint() const
{
     if (acceptor_->is_open())
         return acceptor_->local_endpoint();
    return boost::asio::ip::tcp::endpoint();
}

void WfdeInterface::Start(Pipeline& pipeline, handler_t onConnected)
{
    WAR_ASSERT(onConnected && "Must have a callback");
    WAR_ASSERT(!acceptor_ || !acceptor_->is_open());

    on_connected_ = onConnected;
    pipeline_ = &pipeline;

    acceptor_.reset(new boost::asio::ip::tcp::acceptor(pipeline_->GetIoService()));
    acceptor_->open(endpoint_.protocol());
    acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_->bind(endpoint_);
    acceptor_->listen();

    WAR_ASSERT(acceptor_->is_open());

    LOG_NOTICE << "Starting " << *this << ", listening on " << GetEndpoint();

    boost::asio::spawn(pipeline_->GetIoService(),
                       std::bind(&WfdeInterface::Accept,
                                 this, std::placeholders::_1));
}

void WfdeInterface::Stop()
{
    LOG_NOTICE << "Stopping " << *this;

    if (acceptor_->is_open()) {
       acceptor_->close();
    }
}

void WfdeInterface::Accept(boost::asio::yield_context yield)
{
    auto& threadpool = GetIoThreadpool();

    while(acceptor_->is_open()) {
        boost::system::error_code ec;

        auto& some_pipeline = threadpool.GetAnyPipeline();
#ifdef WFDE_WITH_TLS
        auto socket = make_shared<tls_tcp_socket_t>(some_pipeline);
#else
        auto socket = make_shared<tcp_socket_t>(some_pipeline);
#endif
        LOG_TRACE1_FN << "Created socket " << *socket
            << " on " << some_pipeline
            << " from " << *this;

        acceptor_->async_accept(const_cast< boost::asio::ip::tcp::socket&>(
            socket->GetSocket()), yield[ec]);

        if (!ec) {
            LOG_TRACE1_FN << "Incoming connection: " << *socket;
            try {
                // Switch to the thread assigned to this socket
                some_pipeline.Dispatch({[this, socket](){
                  on_connected_(socket);
                }, "Call on_connected"});
            } WAR_CATCH_ERROR;
        } else {
            LOG_WARN_FN << "Accept error: " << ec;
        }
    }
}

} // namespace impl

Interface::ptr_t CreateInterface(Protocol *parent,
    const std::string& name,
    const boost::asio::ip::tcp::endpoint& endpoint,
    Configuration::ptr_t conf)
{
    return make_shared<impl::WfdeInterface>(parent, endpoint, conf);
}

} // namespace wfde
} // namespace war
