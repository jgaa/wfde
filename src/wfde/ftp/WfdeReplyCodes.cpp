#include <war_wfde.h>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include "wfde/ftp_protocol.h"

using namespace std::string_literals;
using namespace std;

namespace war {
namespace wfde {

const std::pair<int, std::string>& Resolve(const FtpReplyCodes& code)
{
    static const std::vector<std::pair<int, std::string>> messages = {
        {110, "Restart marker."s},
        {125, "Data connection already open; transfer starting."s},
        {150, "File status okay; about to open data connection."s},
        {200, "Command okay."s},
        {211, "Status okay."s},
        {212, "Directory Status."s},
        {213, "RC_FILE_STATUS"s},
        {214, "RC_HELP"s},
        {215, "RC_NAME"s},
        {220, "Service ready for new user."s},
        {221, "Service closing control connection."s},
        {225, "Data connection open; no transfer in progress."s},
        {226, "Closing data connection."s},
        {227, "RC_PASSIVE"s},
        {230, "User logged in, proceed."s},
        {234, "Security data exchange complete."s},
        {250, "Requested file action okay, completed."s},
        {257, "RC_PATHNAME_CREATED"s},
        {331, "User name okay, need password."s},
        {350, "Requested file action pending further information."s},
        {421, "Service not available, closing control connection."s},
        {425, "Can't open data connection."s},
        {426, "Connection closed; transfer aborted."s},
        {450, "Requested file action not taken."s},
        {451, "Requested action aborted: local error in processing."s},
        {452, "Requested action not taken. Insufficient storage space in system."s},
        {500, "Syntax error, command unrecognized."s},
        {501, "Syntax error in parameters or arguments."s},
        {502, "Command not implemented."s},
        {503, "Bad sequence of commands."s},
        {504, "Command not implemented for that parameter."s},
        {530, "Not logged in."s},
        {354, "Request denied for policy reasons"s},
        {550, "Requested action not taken. File is unavailable."s},
        {553, "Requested action not taken. File name not allowed."s}
    };

    WAR_ASSERT(static_cast<decltype(messages.size())>(code) < messages.size());
    return messages[static_cast<int>(code)];
}

}} // namespaces

std::ostream& operator << (std::ostream& o, const war::wfde::FtpReplyCodes& code)
{
    const auto& reply = war::wfde::Resolve(code);
    return o << reply.first << ' ' << reply.second;
}
