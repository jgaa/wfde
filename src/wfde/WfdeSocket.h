#pragma once

#include <wfde/wfde.h>
#include <warlib/WarPipeline.h>
#include <warlib/uuid.h>
#include <warlib/WarLog.h>

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
    : socket_(pipeline.GetIoService()), id_{get_uuid_as_string()}
    ,  pipeline_{pipeline}
    {
    }

    WfdeSocket(Pipeline& pipeline, socket_t&& socket)
    : socket_{std::move(socket)}, id_{get_uuid_as_string()}
    , pipeline_{pipeline}
    {
    }

    WfdeSocket(Pipeline& pipeline,
        const boost::asio::ip::tcp::endpoint::protocol_type & protocol)
    : socket_(pipeline.GetIoService(), protocol)
    ,  id_{get_uuid_as_string()}, pipeline_{pipeline}
    {
    }

    boost::asio::ip::tcp::socket& GetSocket() override { return socket_; }
    const boost::asio::ip::tcp::socket& GetSocket() const override {
        return socket_;
    }
    Pipeline& GetPipeline() override { return pipeline_; };
    const Pipeline& GetPipeline() const override { return pipeline_;}
    const std::string& GetName() const override { return id_; }

    void AsyncConnect(const boost::asio::ip::tcp::endpoint& ep,
                      boost::asio::yield_context& yield) override {
        socket_.async_connect(ep, yield);
    }

    std::size_t AsyncRead(boost::asio::mutable_buffer buffers,
                          boost::asio::yield_context& yield) override {
        WAR_LOG_FUNCTION;
        return boost::asio::async_read(socket_, buffers, yield);
    }

    std::size_t AsyncReadSome(boost::asio::mutable_buffer buffers,
                              boost::asio::yield_context& yield) override {
        WAR_LOG_FUNCTION;
        return socket_.async_read_some(buffers, yield);
    }

    void AsyncWrite(const boost::asio::const_buffer& buffers,
                    boost::asio::yield_context& yield) override {
        WAR_LOG_FUNCTION;

        LOG_DEBUG_FN << "Sending " << boost::asio::buffer_size(buffers)
            << " bytes " << *this;

        boost::asio::async_write(socket_, buffers, yield);
    }

    void AsyncWrite(const write_buffers_t& buffers,
                    boost::asio::yield_context& yield) override {
        WAR_LOG_FUNCTION;

        LOG_DEBUG_FN << "Sending " << boost::asio::buffer_size(buffers)
            << " bytes " << *this;

        boost::asio::async_write(socket_, buffers, yield);
    }

    void AsyncShutdown(boost::asio::yield_context& yield) override {
        // Do nothing.
    }

    void Close() override {
        socket_.close();
    }

    int GetSocketVal() const override {
        return static_cast<int>(
            const_cast<WfdeSocket *>(this)->socket_.native_handle());
    }

    bool IsOpen() const override {
        return socket_.is_open();
    }

    boost::asio::ip::tcp::endpoint GetLocalEndpoint() override {
        try {
            return socket_.local_endpoint();
        } WAR_CATCH_ERROR;

        return boost::asio::ip::tcp::endpoint{};
    }

    virtual boost::asio::ip::tcp::endpoint GetRemoteEndpoint() {
        try {
            return socket_.remote_endpoint();
        } WAR_CATCH_ERROR;

        return boost::asio::ip::tcp::endpoint{};
    }

    void UpgradeToTls(boost::asio::yield_context& yield) override {
        WAR_ASSERT(false && "Not implemented");
        WAR_THROW_T(ExceptionNotImplemented, "Not implemented");
    }

    boost::filesystem::path GetCertPath() {
        return {};
    }

protected:
    socket_t socket_;
    const std::string id_;
    Pipeline& pipeline_;
};

} // namespace impl

using tcp_socket_t = impl::WfdeSocket<boost::asio::ip::tcp::socket>;

} // namespace wfde
} // namespace war
