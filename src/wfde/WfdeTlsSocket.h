#pragma once

#include <wfde/config.h>
#ifdef WFDE_WITH_TLS

#include <boost/asio/ssl.hpp>

#include <wfde/wfde.h>

#include <warlib/WarPipeline.h>
#include <warlib/uuid.h>
#include <warlib/WarLog.h>

/* At this point boost::asio does not allow us to upgrade
 * from plain TCP to SSL by constructing an ssl stream over an
 * existing tcp socket. It appears that the ssl stream must own
 * the underlaying socket.
 *
 * That means that we need to construct a SSL stream for each TCP socket that
 * may need to use SSL at some later time.
 */

namespace war {
namespace wfde {
namespace impl {

template <typename SocketT>
class WfdeTlsSocket : public Socket
{
public:
    using ptr_t = std::shared_ptr<WfdeTlsSocket>;
    using wptr_t = std::weak_ptr<WfdeTlsSocket>;
    using socket_t = SocketT;
    using ssl_socket_t = boost::asio::ssl::stream<socket_t>;

    WfdeTlsSocket(Pipeline& pipeline, const boost::filesystem::path& certPath)
    : id_{get_uuid_as_string()}
    , pipeline_{ pipeline }, cert_path_{certPath}
    {
        InitSocket();
    }


////     Not supported by asio at this time!
//     WfdeTlsSocket(Socket&& origin)
//     : tls_context_{boost::asio::ssl::context::sslv23_server}
//     , socket_{std::move(origin.GetSocket())}
//     , ssl_socket_(socket_, tls_context_)
//     , id_{origin.GetName()}
//     , pipeline_{origin.GetPipeline()}
//     {
//     }


    boost::asio::ip::tcp::socket& GetSocket() override {
        return  static_cast<socket_t&>(ssl_socket_->lowest_layer());
    }
    const boost::asio::ip::tcp::socket& GetSocket() const override {
        return  static_cast<const socket_t&>(ssl_socket_->lowest_layer());
    }
    Pipeline& GetPipeline() override { return pipeline_; };
    const Pipeline& GetPipeline() const override { return pipeline_;}
    const std::string& GetName() const override { return id_; }

    void AsyncConnect(const boost::asio::ip::tcp::endpoint& ep,
                      boost::asio::yield_context& yield) override {

         LOG_TRACE4_F_FN(war::log::LA_IO) << "Connecting to  "
            << ep
            << " from Socket " << GetName();

        GetSocket().async_connect(ep, yield);
    }

    std::size_t AsyncRead(boost::asio::mutable_buffers_1 buffers,
                          boost::asio::yield_context& yield) override {
        WAR_LOG_FUNCTION;

        LOG_TRACE4_F_FN(war::log::LA_IO) << "Receiving up to  "
            << boost::asio::buffer_size(buffers)
            << " bytes " << *this;

        if (using_tls_)
            return boost::asio::async_read(*ssl_socket_, buffers, yield);

        return  boost::asio::async_read(GetSocket(), buffers, yield);
    }

    std::size_t AsyncReadSome(boost::asio::mutable_buffers_1 buffers,
                              boost::asio::yield_context& yield) override {
        WAR_LOG_FUNCTION;
        LOG_TRACE4_F_FN(war::log::LA_IO) << "Receiving up to  "
            << boost::asio::buffer_size(buffers)
            << " bytes " << *this;

        if (using_tls_)
            return ssl_socket_->async_read_some(buffers, yield);

        return GetSocket().async_read_some(buffers, yield);
    }

    void AsyncWrite(const boost::asio::const_buffers_1& buffers,
                    boost::asio::yield_context& yield) override {
        WAR_LOG_FUNCTION;
        LOG_TRACE4_F_FN(war::log::LA_IO) << "Sending "
            << boost::asio::buffer_size(buffers)
            << " bytes " << *this;

        if (using_tls_)
            boost::asio::async_write(*ssl_socket_, buffers, yield);
        else
            boost::asio::async_write(GetSocket(), buffers, yield);
    }

    void AsyncWrite(const write_buffers_t& buffers,
                    boost::asio::yield_context& yield) override {
        WAR_LOG_FUNCTION;
        LOG_TRACE4_F_FN(war::log::LA_IO) << "Sending "
            << boost::asio::buffer_size(buffers)
            << " bytes " << *this;

        if (using_tls_)
            boost::asio::async_write(*ssl_socket_, buffers, yield);
        else
            boost::asio::async_write(GetSocket(), buffers, yield);
    }

    void AsyncShutdown(boost::asio::yield_context& yield) override {
        WAR_LOG_FUNCTION;
        if (!using_tls_)
            return;

        ssl_socket_->async_shutdown(yield);
        using_tls_ = false;
    }

    void Close() override {
        WAR_LOG_FUNCTION;
        GetSocket().close();
    }

    int GetSocketVal() const override {
        return static_cast<int>(const_cast<WfdeTlsSocket *>(this)->
            GetSocket().native_handle());
    }

    bool IsOpen() const override {
        return GetSocket().is_open();
    }

    boost::asio::ip::tcp::endpoint GetLocalEndpoint() override {
        try {
            return GetSocket().local_endpoint();
        } WAR_CATCH_ERROR;

        return boost::asio::ip::tcp::endpoint{};
    }

    virtual boost::asio::ip::tcp::endpoint GetRemoteEndpoint() {
        try {
            return GetSocket().remote_endpoint();
        } WAR_CATCH_ERROR;

        return boost::asio::ip::tcp::endpoint{};
    }

    void UpgradeToTls(boost::asio::yield_context& yield) override {
        WAR_LOG_FUNCTION;

        LOG_TRACE2_FN << "Upgrading " << *this << " To TLS";

        ssl_socket_->async_handshake(boost::asio::ssl::stream_base::server,
                                    yield);
        using_tls_ = true;
    }

    boost::filesystem::path GetCertPath() {
        return cert_path_;
    }

private:
    void InitSocket() {
        tls_context_.set_options(boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::single_dh_use);
        tls_context_.use_certificate_chain_file(cert_path_.c_str());
        tls_context_.use_private_key_file(cert_path_.c_str(),
                                          boost::asio::ssl::context::pem);
        //tls_context_.use_tmp_dh_file("dh512.pem");

        ssl_socket_ = std::make_unique<ssl_socket_t>(pipeline_.GetIoService(),
                                                     tls_context_);
    }

    const std::string id_;
    Pipeline& pipeline_;
    bool using_tls_ = false;
    boost::asio::ssl::context tls_context_{ boost::asio::ssl::context::sslv23_server };
    std::unique_ptr<ssl_socket_t> ssl_socket_;
    const boost::filesystem::path cert_path_;
};

} // namespace impl


using tls_tcp_socket_t = impl::WfdeTlsSocket<boost::asio::ip::tcp::socket>;

} // namespace wfde
} // namespace war

#endif // WFDE_WITH_TLS
