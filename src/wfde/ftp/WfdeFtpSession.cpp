#include "war_wfde.h"

#include <tuple>
#include <locale>
#include <chrono>

#include "WfdeProtocolFtp.h"
#include "WfdeFtpSession.h"
#include "log/WarLog.h"
#include "wfde/ftp_protocol.h"
#include "WfdeAsciiFile.h"

#include <boost/iterator/iterator_concepts.hpp>
#include "boost/regex.hpp"
#include <boost/algorithm/string.hpp>

#include "war_helper.h"

using namespace std;
using namespace std::string_literals;

std::ostream& operator << (std::ostream& o,
                           const war::wfde::impl::WfdeFtpSession& ses) {
    return o << "{ FTP Session " << ses.GetId() << " }";
}

namespace war {
namespace wfde {
namespace impl {


/////////////////////// Class WfdeFtpSessionInput ////////////////////////

boost::string_ref WfdeFtpSessionInput::FetchNextCommand()
{
    bool skip_first_read = false;
    if (bytes_leftover_) {
        memmove(&input_buffer_[0], current_buffer_, bytes_leftover_);
        current_buffer_ = &input_buffer_[0] + bytes_leftover_;
        skip_first_read = true;
    } else {
        current_buffer_ = &input_buffer_[0];
    }

    while(true) {
        auto bytes_to_read = &input_buffer_[input_buffer_.size()]
            - current_buffer_;

        if (ftp_ses_) {
            LOG_TRACE4_FN << "Ready to read up to " << bytes_to_read
                << " from client " << *ftp_ses_;
        }

        size_t bytes_read = 0;
        if (UNLIKELY(skip_first_read)) {
            skip_first_read = false;
        } else {
            bytes_read = ReadSome(current_buffer_, bytes_to_read);

            if (!bytes_read) {
                WAR_THROW_T(NoInputException, "No input");
            } else {
                LOG_TRACE2_FN << "Read " << bytes_read
                    << " bytes from socket in " << *ftp_ses_;
            }
        }

        // See if we have a complete command
        // TODO: Implemet the obsolete telnet command characters to edit the input buffer
        const auto prev_bytes_read = current_buffer_ - &input_buffer_[0];
        boost::string_ref cmd_buffer(&input_buffer_[0],
                                     bytes_read + prev_bytes_read);
        const auto eob = cmd_buffer.find_first_of(crlf_);
        if (eob == cmd_buffer.npos) {
            LOG_TRACE3_FN << "No CRLF in the received buffer.";
            if ((bytes_read + prev_bytes_read) >= (input_buffer_.size() -1)) {
                WAR_THROW_T(InputTooBigException, "Overflow");
            }

            // Let's assume that the client sent a partial command
            current_buffer_ += bytes_read;
            continue;
        }

         // Send the command to the parser and execute it
        boost::string_ref request(&input_buffer_[0], eob);

        // Deal with valid input after the crlf sequence
        if ((bytes_leftover_ = cmd_buffer.size() - request.size() - 2) > 0) {
            current_buffer_ = &input_buffer_[0] + request.size() + 2;
        }

        return request;
    }
}

size_t WfdeFtpSessionInput::ReadSome(char *p, const std::size_t bytes)
{
    return ftp_ses_->socket_.async_read_some(boost::asio::buffer(p,bytes),
                                   (*ftp_ses_->yield_)[ftp_ses_->ec_]);
}


/////////////////////// Class WfdeFtpSession ////////////////////////

WfdeFtpSession::WfdeFtpSession(const Session::ptr_t& session)
: input_(this)
, session_(session)
, id_(session->GetUuid())
, socket_ptr_{session->GetSocket().shared_from_this()}
, socket_{socket_ptr_->GetSocket()}
, ftp_commands_{GetDefaultFtpCommands()}
{
    LOG_TRACE1_FN << "Session " << *this << " is constructed";
}

WfdeFtpSession::~WfdeFtpSession()
{
    LOG_TRACE1_FN << "Session " << *this << " is destructed";
}

void WfdeFtpSession::ProcessCommands(boost::asio::yield_context yield)
{
    yield_ = make_unique<boost::asio::yield_context>(move(yield));

    if (auto sesref = session_.lock()) {
        LOG_NOTICE << "Starting FTP " << *sesref
            << " on " << sesref->GetProtocol().GetHost()
            << " with " << sesref->GetSocket();
    } else {
        LOG_WARN_FN << "The FTP session " << id_
            << " ended before we got started";
        DoClose();
        return;
    }

    // Say hello
    Reply(FtpReplyCodes::RC_READY);

    // Process commands
    while(!close_pending_ && !ec_ && !session_.expired() && socket_.is_open()) {

        try {
            const auto request = input_.FetchNextCommand();

            LOG_TRACE1_FN << "Received FTP request " << log::Esc(request)
                << " from " << *this;

            ProcessRequest(request);

        } catch(const WfdeFtpSessionInput::InputTooBigException&) {
            LOG_WARN_FN << "Received a full input buffer without CRLF. "
                << "Dropping the connection on " << *this;

            // TODO: Add statistics
            // TODO: Possible hacker attack - Notify the security manager
            Reply(FtpReplyCodes::RC_BYE,
                    "Command too long. Did you forget to send CRLF? Bye.");
            break;
        } catch(const WfdeFtpSessionInput::NoInputException&) {
            LOG_DEBUG_FN << "Failed to read from socket "
                << ec_ << " in " << *this;
            break;
        }
    }

    DoClose();
}

void WfdeFtpSession::ProcessRequest(const boost::string_ref& request)
{
    try {
        auto& cmd = GetFtpCommand(request);
        OnCommand(cmd, request);
    } catch (const ExceptionParseError& ex) {
        LOG_DEBUG_FN << "Caught exception: " << ex;
        Reply(FtpReplyCodes::RC_SYNTAX_ERROR);
    } WAR_CATCH_ERROR;
}


FtpCmd& WfdeFtpSession::GetFtpCommand(const boost::string_ref& request)
{
    locale loc;
    string cmd_name;

    for(auto ch = request.begin()
        ; (ch != request.end()) && isalpha(*ch, loc)
        ; ++ch) {
        cmd_name += toupper(*ch, loc);
    }

    if (cmd_name.empty()) {
        WAR_THROW_T(ExceptionParseError, "No command name");
    }

    if (cmd_name.size() > max_cmd_name_length_) {
        WAR_THROW_T(ExceptionParseError, "Command name too long.");
        // TODO: Potential hacker attack - Notify the security manager
    }

    auto my_cmd = ftp_commands_.find(cmd_name);
    if (my_cmd == ftp_commands_.end()) {
        WAR_THROW_T(ExceptionParseError, "Unrecognized command");
    }

    curr_cmd_name_ = cmd_name;

    return my_cmd->second;
}

void WfdeFtpSession::OnCommand(FtpCmd& cmd, const boost::string_ref& request)
{
    auto session = session_.lock();
    if (!session) {
        return;
    }

    if (!cmd.NeedPrevCmd().empty()) {
        if (prev_cmd_name_ != cmd.NeedPrevCmd()) {
            LOG_NOTICE_FN << "Bad sequence of commands for " << *this
                << ". I require this command " << log::Esc(curr_cmd_name_)
                << " to follow the previous command " << log::Esc(prev_cmd_name_);

           Reply(FtpReplyCodes::RC_BAD_SEQUENCE_OF_COMMANDS);
           return;
        }
    }

    if (cmd.MustBeLoggedIn() && !state_.IsLoggedIn()) {
        LOG_NOTICE_FN << "The command " << log::Esc(curr_cmd_name_)
                << " Requiers the user to be logged in. Saying no-no to "
                << *this;
        Reply(FtpReplyCodes::RC_NOT_LOGGED_ON);
        return;
    }

    if (cmd.MustNotBeLoggedIn() && state_.IsLoggedIn()) {
        LOG_NOTICE_FN << "The command " << log::Esc(curr_cmd_name_)
                << " Requiers the user NOT to be logged in. Saying no-no to "
                << *this;
        Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN, "You are logged in!");
        return;
    }

    if (cmd.MustBeInTransfer() && !state_.IsInTransfer()) {
        LOG_NOTICE_FN << "The command " << log::Esc(curr_cmd_name_)
                << " Requiers the user to be in file transfer. Saying no-no to "
                << *this;
        Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN, "No active file transfer");
        return;
    }

    if (cmd.MustNotBeInTransfer() && state_.IsInTransfer()) {
        LOG_NOTICE_FN << "The command " << log::Esc(curr_cmd_name_)
                << " Requiers the user NOT to be in file transfer. Saying no-no to "
                << *this;
        Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN, "Active file transfer!");
        return;
    }

    if (cmd.NeedPostOrPasv() && !state_.HavePasvOrPost()) {
        LOG_DEBUG_FN << "STOR before POST or PASV. Rejecting command"
            << " on " << session;

        Reply(FtpReplyCodes::RC_BAD_SEQUENCE_OF_COMMANDS,
              "Need PASV or POST before "s + cmd.GetName());
        return;
    }

    // Ugly and error-prone way to get a pointer to the start of the command parameters
    // TODO: Replace with something better.
    auto remaining = request.size();
    auto start_of_param = request.begin();
    start_of_param += curr_cmd_name_.size();
    remaining -= curr_cmd_name_.size();
    while(remaining && (*start_of_param == ' ')) {
        ++start_of_param;
        --remaining;
    }

    const boost::string_ref param(start_of_param, remaining);
    const std::string wasted_buffer_workaround_for_broken_regex(param.data(), param.length());
    FtpCmd::match_t param_match;
    if (!cmd.CanHaveParams()
        || boost::regex_match(wasted_buffer_workaround_for_broken_regex,
                              param_match, cmd.GetRegex())) {

        LOG_DEBUG_FN << "Executing FTP command " << cmd.GetName() << ' '
            << log::Esc(param) << " on " << *this;

        try {
            FtpReply reply;
            cmd.OnCmd(*session, state_, request, param, param_match, reply);
            session->Touch();
            prev_cmd_name_ = cmd.GetName();

            if (reply.HaveReply()) {
                auto cmd_replied = reply.GetReply();
                Reply(cmd_replied.first, cmd_replied.second, reply.IsMultiline());
                if (reply.NeedToEndSession()) {
                    LOG_DEBUG_FN << "The client requested to end " << *this;
                    Close();
                }
                return;
            }
            if (state_.abort_pending) {
                if (abort_transfer_) {
                    abort_transfer_();
                    abort_transfer_ = nullptr; // Only once
                }
            }
        }  WAR_CATCH_ALL_EF(
            Reply(FtpReplyCodes::RC_BYE, "Internal server error");
            close_pending_ = true;
        )
    } else {
        LOG_WARN_FN << "Invalid argument " << log::Esc(param)
            << " to FTP command " << log::Esc(curr_cmd_name_)
            << " using regex " << cmd.GetRegex();
        Reply(FtpReplyCodes::RC_SYNTAX_ERROR_IN_PARAMS);
    }

}

void WfdeFtpSession::Reply(FtpReplyCodes code,
                           const string& message,
                           bool multiLine)
{
    Reply(code, *yield_, message, multiLine);
}

void WfdeFtpSession::Reply(FtpReplyCodes code,
                           boost::asio::yield_context& yield,
                           const string& message,
                           const bool multiLine)
{
    static const string end{" END\r\n"s}, dash{"- "s}, space{" "s};

    if (reply_failed_ || !socket_.is_open()) {
        LOG_DEBUG_FN << "The socket is dead. No reply is expected.";
        return;
    }

    // Put the reply in the text-buffer;
    reply_strings_.resize(1);
    const auto& reply = Resolve(code);
    reply_strings_[0] = to_string(reply.first) + (multiLine ? dash : space)
        + (message.empty() ? reply.second : message) + crlf_;
    if (multiLine) {
        reply_strings_[0] += to_string(reply.first) + end;
    }

    const bool logging = log::LogEngine::IsRelevant(war::log::LL_TRACE1, war::log::LA_GENERAL);
    std::string relply_msg_for_log;

    // Prepare the asio message buffer-wrappers
    reply_buffers_.resize(reply_strings_.size());
    for(size_t i = 0; i < reply_strings_.size(); ++i) {
        reply_buffers_[i] = {&reply_strings_[i][0], reply_strings_[i].size()};
        if (logging)
            relply_msg_for_log += reply_strings_[i];
    }


    // Strip off the last CRLF in the copy of the reeply for logging.
    if (logging && !relply_msg_for_log.empty()) {
        WAR_ASSERT(relply_msg_for_log.size() > 2);
        relply_msg_for_log.resize(relply_msg_for_log.size() -2);
    }

    LOG_TRACE1_FN << "Replying " << log::Esc(relply_msg_for_log)
        << " in " << *this;

    // Send the reply
    WAR_ASSERT(reply_failed_ == false);
    boost::asio::async_write(socket_, reply_buffers_, yield[ec_]);
    if (ec_) {
        reply_failed_ = true;
        LOG_WARN_FN << "Failed to reply " << ec_ << " in " << *this;
    }
}

void WfdeFtpSession::Close()
{
    close_pending_ = true;

    if (socket_.is_open()) {
        socket_.close(); // Ent any read operations
    }
}

void WfdeFtpSession::DoClose()
{
    if (!closed_) {
        Close();
        closed_ = true;
        if (auto myses = session_.lock()) {

            LOG_DEBUG_FN << "Closing FTP connection " << *this;

            if (abort_transfer_) {
                LOG_TRACE1_FN << "Calling abort_transfer_ on " << *this;
                abort_transfer_();
            }

            myses->GetProtocol().GetHost().GetSessionManager().CloseSession(id_);
        } else {
            LOG_WARN_FN << *this << " has expired";
        }
    }
}


void WfdeFtpSession::StartTransfer(unique_ptr< File > file)
{
    if (state_.GetType() == FtpState::Type::BIN) {
        current_file_ = move(file);
    } else {
        current_file_ = make_unique<WfdeAsciiFile>(move(file));
    }

    switch(current_file_->GetOperation()) {
        case File::FileOperation::READ:
            state_.transfer = FtpState::Transfer::OUTGOING;
            break;
        case File::FileOperation::APPEND:
        case File::FileOperation::WRITE_NEW:
        case File::FileOperation::WRITE:
            state_.transfer = FtpState::Transfer::INCOMING;
            break;
    }

    boost::asio::spawn(GetPipeline().GetIoService(),
                       bind(&WfdeFtpSession::TransferFile,
                            shared_from_this(),
                            std::placeholders::_1));
}

void WfdeFtpSession::TransferFile(boost::asio::yield_context yield)
{
    WAR_ASSERT(current_file_);

    Reply(FtpReplyCodes::RC_OPENING_CONNECTION, yield);

    if (ec_ || session_.expired() || closed_ || close_pending_) {
        LOG_WARN_FN << "The connection is closed. Silently Aborting before "
            << "starting the transfer of " << *current_file_;
        current_file_.reset();
        state_.ResetTransfer();
        return;
    }

    boost::asio::ip::tcp::socket sck(GetPipeline().GetIoService());

    if (state_.initiation == FtpState::Initiation::PORT) {
        if (!DoPort(sck, yield)) {
            Reply(FtpReplyCodes::RC_CANT_OPEN_DATA_CONN, yield);
            CleanupTransfer();
            return;
        }
    } else { // PASV

        // FIXME: **** If we get here, we need somehow to yield
        //pasv_promise.wait();

        abort_transfer_ = [this, &sck]() {
            LOG_DEBUG << "Closing transfer socket for << " << *this
                << " because the transfer is aborted";

            state_.pasv->GetAcceptor().close();
            if (sck.is_open())
                sck.close();
        };

        boost::system::error_code ec;
        state_.pasv->GetAcceptor().async_accept(sck, yield[ec]);

        if (ec) {
            Reply(FtpReplyCodes::RC_CANT_OPEN_DATA_CONN, yield);
            CleanupTransfer();
            return;
        }
    }

    abort_transfer_ = [this, &sck]() {
        LOG_DEBUG << "Closing transfer socket for << " << *this
            << " because the transfer is aborted";

        if (sck.is_open())
            sck.close();
    };

    try {
        if (state_.transfer == FtpState::Transfer::OUTGOING) {
            SendFile(sck, yield);
        } else {
            ReceiveFile(sck, yield);
        }
    } WAR_CATCH_ALL_EF(
        LOG_WARN_FN << "File transfer of  " << *current_file_
                    << " Failed in " << *this;
        Reply(FtpReplyCodes::RC_TRANSFER_ABORTED, yield);
    );

    abort_transfer_ = nullptr;

    CleanupTransfer();
}

void WfdeFtpSession::SendFile(boost::asio::ip::tcp::socket& sck,
                              boost::asio::yield_context& yield)
{
    File::fpos_t bytes = 0;
    boost::system::error_code ec;

    if (!sck.is_open()) {
        LOG_WARN_FN << "The transfer socket is closed before send on "
            << *this;
        NotifyFailed(yield);
        return;
    }

    WAR_ASSERT(current_file_);
    WAR_ASSERT(sck.is_open());

    LOG_TRACE2_FN << "Entering send-loop for " << *current_file_;

    while(!current_file_->IsEof()) {
        auto buffer = current_file_->Read();
        if (boost::asio::buffer_size(buffer) == 0) {
            continue; // May happen when generators filter content
        }

        boost::asio::async_write(sck, buffer, yield[ec]);

        if (UNLIKELY(session_.expired())) {
            LOG_WARN_FN << *this << "expiered while sending "
                        << *current_file_;
            return;
        }

        if (UNLIKELY(ec)) {
            LOG_WARN_FN << "File transfer of  " << *current_file_
                        << " Failed " << ec << " in " << *this;

            NotifyFailed(yield);
            return;
        }

        bytes += boost::asio::buffer_size(buffer);
        TransferTouch();
    }

    LOG_DEBUG_FN << "File transfer of " << *current_file_ << " suceeded";
    LOG_NOTICE << *this << " successfully sent " << *current_file_
               << ' ' << log::Esc(state_.requested_path_)
               << " (" << bytes << " bytes)";
    Reply(FtpReplyCodes::RC_CLOSING_DATA_CONNECTION, yield,
          "Successfully sent " + state_.requested_path_);
    sck.close();
}

void WfdeFtpSession::NotifyFailed(boost::asio::yield_context& yield)
{
    LOG_TRACE4_FN << "Transfer failed. Will reply. reply_failed_="
        << reply_failed_
        << ", socket_=" << (socket_.is_open() ? "open" : "closed");

    if (reply_failed_ || !socket_.is_open()) {
        LOG_DEBUG_FN << "Cannot reply to client. Connection is dead";
        return;
    }

    Reply(FtpReplyCodes::RC_TRANSFER_ABORTED, yield);

    if (state_.abort_pending) {
        state_.abort_pending = false;

        Reply(FtpReplyCodes::RC_CLOSING_DATA_CONNECTION, yield,
                "ABOR command successful");
    }
}


void WfdeFtpSession::ReceiveFile(boost::asio::ip::tcp::socket& sck,
                                 boost::asio::yield_context& yield)
{
    File::fpos_t bytes = 0;
    boost::system::error_code ec;

    if (!sck.is_open()) {
        LOG_WARN_FN << "The transfer socket is closed before receive on "
            << *this;

        NotifyFailed(yield);
        return;
    }

    WAR_ASSERT(current_file_);
    WAR_ASSERT(sck.is_open());

    LOG_TRACE2_FN << "Entering receive-loop for " << *current_file_;

    for(;;) {
        auto buffer = current_file_->Write();
        //const auto bytes_read = boost::asio::async_read(sck, buffer, yield[ec]);

        const auto bytes_read = sck.async_read_some(buffer, yield[ec]);

        LOG_TRACE4_F_FN(log::LA_IO) << "After async read: bytes_read="
            << bytes_read
            << ", ec=" << ec
            << ", rcvbuffer_size=" << boost::asio::buffer_size(buffer)
            << ' ' << *current_file_;

        if (UNLIKELY(session_.expired())) {
            LOG_WARN_FN << *this << "expiered while sending "
                        << *current_file_;
            return;
        }

        if (UNLIKELY(!sck.is_open())) {
            LOG_WARN_FN << *this << "Socket was unexpectally closed in "
                        << *current_file_;

            Reply(FtpReplyCodes::RC_TRANSFER_ABORTED, yield);
        }

        if (UNLIKELY(ec)) {

            if (ec.value() == boost::asio::error::eof) {
                current_file_->SetBytesWritten(bytes_read);
                LOG_TRACE4_FN << "Got boost::asio::error::eof";
                break;
            }

            LOG_WARN_FN << "File transfer of  " << *current_file_
                << " Failed " << ec << " in " << *this;

            NotifyFailed(yield);
            return;
        }

        current_file_->SetBytesWritten(bytes_read);
        bytes += bytes_read;

        TransferTouch();
    }

    LOG_NOTICE << *this << " successfully received " << *current_file_
               << ' ' << log::Esc(state_.requested_path_)
               << " (" << bytes << " bytes)";
    Reply(FtpReplyCodes::RC_CLOSING_DATA_CONNECTION, yield,
         "Successfully received " + state_.requested_path_
    );
    sck.close();
}

void WfdeFtpSession::TransferTouch()
{
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::steady_clock::duration xduration{now -last_touch_time_};
    if (std::chrono::duration_cast<std::chrono::seconds>(xduration).count()
        > transefer_touch_interval_) {
        auto session = session_.lock();
        session->Touch();
        last_touch_time_ = now;
    }
}

bool WfdeFtpSession::DoPort(boost::asio::ip::tcp::socket& sck,
                            boost::asio::yield_context& yield)
{
    boost::system::error_code ec;
    boost::asio::ip::tcp::endpoint local(socket_.local_endpoint());
    auto remote = state_.GetPortEndpoint();

    // TODO: Deal with options for specifying the local port and IP
    try {
        if (local.protocol() == remote.protocol()) {
            sck.open(local.protocol());
            // FIXME: bind() fails with "Address already in use"
            //sck.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            //sck.bind(local);
        } // TODO: Do we allow the user to specify another protocol than
            // he connected with to the server?
    } WAR_CATCH_ALL_EF (
        return false;
    );


    LOG_DEBUG_FN << "Preparing data-connection from " << local
        << " to " << remote
        << " on " << *this;

    sck.async_connect(state_.GetPortEndpoint(), yield[ec]);

    if (UNLIKELY(ec)) {
        WAR_ASSERT(current_file_);
        LOG_WARN_FN << "Failed to connect from " << local
            << " to " << remote
            << ": " << ec
            << " on " << *this
            << " while trying to transfer "
            << *current_file_;
        return false;
    }

    if (UNLIKELY(session_.expired())) {
        LOG_WARN_FN << *this << "expiered while trying to transfer "
            << *current_file_
            << " with reote endpoint " << state_.GetPortEndpoint();
        return false;
    }

    return true;
}


void WfdeFtpSession::CleanupTransfer()
{
    current_file_.reset();
    state_.ResetTransfer();
}


}}} // Namespaces
