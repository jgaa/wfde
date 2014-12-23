#pragma once

#include "wfde/wfde.h"
#include "tasks/WarPipeline.h"
#include "war_uuid.h"

namespace war {
namespace wfde {
namespace impl {

template <typename SocketT>
class WfdeSocket : public Socket
{
public:
    using ptr_t = std::shared_ptr<WfdeSocket>;
    using wptr_t = std::weak_ptr<WfdeSocket>;
    using socket_t = SocketT;

    WfdeSocket(Pipeline& pipeline)
    : socket_(pipeline.GetIoService()), id_{get_uuid_as_string()},
      pipeline_{pipeline}
    {
    }


    WfdeSocket(Pipeline& pipeline,
        const boost::asio::ip::tcp::endpoint::protocol_type & protocol)
    : socket_(pipeline.GetIoService(), protocol),
      id_{get_uuid_as_string()}, pipeline_{pipeline}
    {
    }

    boost::asio::ip::tcp::socket& GetSocket() override { return socket_; }
    const boost::asio::ip::tcp::socket& GetSocket() const override {
        return socket_;
    }
    Pipeline& GetPipeline() override { return pipeline_; };
    const Pipeline& GetPipeline() const override { return pipeline_;}
    const std::string& GetName() const override { return id_; }

protected:
    socket_t socket_;
    const std::string id_;
    Pipeline& pipeline_;
};

} // namespace impl

using tcp_socket_t = impl::WfdeSocket<boost::asio::ip::tcp::socket>;

} // namespace wfde
} // namespace war
