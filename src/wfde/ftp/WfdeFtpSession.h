#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <deque>
#include <unordered_map>
#include <string>

#include <boost/asio/spawn.hpp>

#include "wfde/wfde.h"
#include "wfde/ftp_protocol.h"
#include "tasks/WarPipeline.h"
#include "log/WarLog.h"
#include "war_uuid.h"


using namespace std;
using namespace std::string_literals;

namespace war {
namespace wfde {
namespace impl {

class WfdeFtpSession;

class WfdeFtpSessionInput
{
public:
    struct InputTooBigException : public ExceptionBase {};
    struct NoInputException : public ExceptionBase {};

    WfdeFtpSessionInput(WfdeFtpSession *ftpSes) : ftp_ses_{ftpSes} {}

    boost::string_ref FetchNextCommand();
protected:
    virtual std::size_t ReadSome(char *p, const std::size_t bytes);
private:
    const std::string crlf_ = "\r\n"s;
    std::array<char, 1024*16> input_buffer_; // Control channel input buffer
    WfdeFtpSession *ftp_ses_;
    char * current_buffer_ = &input_buffer_[0];
    size_t bytes_leftover_ = 0;
};

/*! Implementation of a FTP protocol session.
 *
 * This class contains the actual FTP protocol implementation,
 * along with the session specific data for an individual session.
 */
class WfdeFtpSession : public Session::SessionData,
    public std::enable_shared_from_this<WfdeFtpSession>
{
public:
    WfdeFtpSession(const Session::ptr_t& session);

    ~WfdeFtpSession();

    /********** Overrides for SessionData ********/

    void StartTransfer(std::unique_ptr<File> file) override;

    void StartTls() override;

    /********** End overrides for SessionData ********/

    void ProcessCommands(boost::asio::yield_context yield);

    const boost::uuids::uuid& GetId() const { return id_; }

protected:
    /*! Prepare the first line of the reply.*/
    virtual void Reply(FtpReplyCodes code, const std::string& message = "",
                       bool multiLine = false);

    /*! Prepare the first line of the reply.*/
    virtual void Reply(FtpReplyCodes code,
                       boost::asio::yield_context& yield,
                       const std::string& message = "",bool multiLine = false);

    /*! Close the session. Clean up. */
    virtual void Close();

    Session::ptr_t GetSession() {
        if (auto ses = session_.lock()) {
            return ses;
        }
        WAR_THROW_T(ExceptionWeakPointerHasExpired, "Session");
    }

    Pipeline& GetPipeline() {
        return GetSession()->GetSocket().GetPipeline();
    }

    bool closed_ = false;
private:
    void DoClose();

    friend class WfdeFtpSessionInput;
    void ProcessRequest(const boost::string_ref& request);
    void OnCommand(FtpCmd& cmd, const boost::string_ref& request);
    FtpCmd& GetFtpCommand(const boost::string_ref& request);
    void TransferFile(boost::asio::yield_context yield);
    void SendFile(boost::asio::yield_context& yield);
    void ReceiveFile(boost::asio::yield_context& yield);
    void CleanupTransfer();
    bool DoPort(Socket& sck,
                boost::asio::yield_context& yield);
    void TransferTouch();
    void NotifyFailed(boost::asio::yield_context& yield);

    WfdeFtpSessionInput input_;
    const Session::wptr_t session_; // The SessionManagers session we are bound to
    const boost::uuids::uuid id_; // Same as session_.lock()->GetUuid()
    const Socket::ptr_t socket_ptr_; // We need to keep the socket alive
    Socket::write_buffers_t reply_buffers_; // asio buffer wrappers
    std::vector<std::string> reply_strings_; // The actual data to send
    std::unique_ptr<boost::asio::yield_context> yield_; // Control connection only!
    const FtpCmd::ftp_commands_t ftp_commands_; // The FTP commands we know about
    std::string prev_cmd_name_;
    std::string curr_cmd_name_;
    FtpState state_; // Transfer state
    const std::size_t max_cmd_name_length_ = 8; // Reasonable max value for command-name
    unique_ptr<File> current_file_; // File being transferred
    std::function<void()> abort_transfer_;
    const std::string crlf_ = "\r\n"s;
    bool close_pending_ = false;
    std::chrono::steady_clock::time_point last_touch_time_{};
    const int transefer_touch_interval_ = 5; // Call Touch() on session every n seconds
    bool reply_failed_ = false;
    std::shared_ptr<Socket> transfer_sck_;
};

}}} // naamespaces

std::ostream& operator << (std::ostream& o, const war::wfde::impl::WfdeFtpSession& ses);
