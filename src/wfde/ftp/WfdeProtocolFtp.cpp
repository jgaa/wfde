#include "war_wfde.h"
#include "WfdeProtocolFtp.h"
#include "WfdeFtpSession.h"

#include "war_helper.h"

using namespace std;

namespace war {
namespace wfde {
namespace impl {

WfdeProtocolFtp::WfdeProtocolFtp(Host *parent, const Configuration::ptr_t& conf)
: WfdeProtocol(parent, conf)
{
}

WfdeProtocolFtp::~WfdeProtocolFtp()
{
}

void WfdeProtocolFtp::HandleConnection(const Socket::ptr_t& socket)
{
    LOG_DEBUG_FN << "Incoming connection from " << *socket;

    SessionManager::SessionParams ses_data;
    ses_data.protocol = shared_from_this();
    ses_data.socket = socket;

    auto session = GetHost().GetSessionManager().CreateSession(ses_data);
    auto ftp_session = make_shared<WfdeFtpSession>(session);

    session->SetPermissions(GetEffectivePermissions());

    auto my_ftp_session = ftp_session.get();
    session->Add(ftp_session);
    session->Set(my_ftp_session);

    auto& ios = socket->GetPipeline().GetIoService();

    /* The pipeline works as a task sequencer.
     */
    boost::asio::spawn(ios, bind(&WfdeFtpSession::ProcessCommands,
                                 ftp_session,
                                 std::placeholders::_1));
}



void WfdeProtocolFtp::RegisterProtocol()
{
    RegisterProtocolFactory("FTP", [](Host * parent,
                            const Configuration::ptr_t& conf) {
        return make_shared<WfdeProtocolFtp>(parent, conf);
    });
}


} //
} // wfde
} // war

