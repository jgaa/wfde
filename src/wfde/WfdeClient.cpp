
#include "war_wfde.h"

#include <boost/uuid/uuid_io.hpp>

#include "tasks/WarThreadpool.h"
#include "log/WarLog.h"

#include "WfdeClient.h"
#include "WfdeSession.h"

using namespace std;

std::ostream& operator << (std::ostream& o, const war::wfde::Client& client) {
    return o << '{' << "Client " << war::log::Esc(client.GetLoginName())
    << ' ' << client.GetUuid() << '}';
}

namespace war {
namespace wfde {
namespace impl {

WfdeClient::WfdeClient(std::string loginName, const boost::uuids::uuid& uuid)
: login_name_(move(loginName)), uuid_(uuid)
{
}

WfdeClient::~WfdeClient()
{
    WAR_ASSERT(instances_ == 0);
}

} // impl

} // wfde
} // war
