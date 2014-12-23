#pragma once

#include "war_wfde.h"
#include "WfdeProtocol.h"


namespace war {
namespace wfde {
namespace impl {

class WfdeProtocolFtp : public WfdeProtocol
{
public:
    using ptr_t = std::shared_ptr<WfdeProtocol>;
    using wptr_t = std::weak_ptr<WfdeProtocol>;

    WfdeProtocolFtp(Host *parent, const Configuration::ptr_t& conf);
    ~WfdeProtocolFtp();

    /*! Convenience function to register a class factory
     *
     * This register the "FTP" prtotocol
     */
    static void RegisterProtocol();

protected:
    void HandleConnection(const Socket::ptr_t& socket) override;


private:

};

} // namespace impl
} // namespace wfde
} // namespace war
