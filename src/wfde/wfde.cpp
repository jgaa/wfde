
#include <iostream>

#include "wfde/wfde.h"
#include "ftp/WfdeProtocolFtp.h"


std::ostream& operator << (std::ostream& o, const war::wfde::Version& ver)
{
    return o
        << static_cast<int>(war::wfde::Version::MAJOR)
        << '.'
        << static_cast<int>(war::wfde::Version::MINOR);
}

namespace war {
namespace wfde {

void RegisterDefaultProtocols()
{
    impl::WfdeProtocolFtp::RegisterProtocol();
}

} // wfde
} // war
