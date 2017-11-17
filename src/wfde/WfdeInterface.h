#pragma once

#include <thread>
#include <wfde/wfde.h>
#include "WfdeEntity.h"

namespace war {
namespace wfde {
namespace impl {

class WfdeInterface : public Interface,
    public WfdeEntity<Protocol, WfdeInterface, Entity>
{
public:
    using ptr_t = std::shared_ptr<WfdeInterface>;
    using wptr_t = std::weak_ptr<WfdeInterface>;

    WfdeInterface(Protocol *parent,
                  const boost::asio::ip::tcp::endpoint& endpoint,
                  Configuration::ptr_t& conf);
    ~WfdeInterface();

    boost::asio::ip::tcp::endpoint GetEndpoint() const override;

    children_t GetChildren(const std::string& filter) const override {
        return children_t{}; // No children for this class
    }

    unsigned AddInterfaces();
    void Start(Pipeline& pipeline, handler_t onConnected) override;
    void Stop() override;

protected:
    void Accept(boost::asio::yield_context yield);

    const boost::asio::ip::tcp::endpoint endpoint_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    handler_t on_connected_;
    Pipeline *pipeline_{nullptr};
    boost::filesystem::path tls_cert_;
};

} // namespace impl
} // namespace wfde
} // namespace war
