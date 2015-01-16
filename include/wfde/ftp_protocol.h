#pragma once

#include <iosfwd>
#include <unordered_map>
#include <future>

#include <boost/regex.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "war_error_handling.h"

namespace war {
namespace wfde {

enum class FtpReplyCodes
{
    RC_RESTART_MARKER /*110*/,
    RC_CONNECTION_OPEN_STARTING_XFER /*125*/,
    RC_OPENING_CONNECTION /*150*/,
    RC_OK /*200*/,
    RC_STATUS_OK /*211*/,
    RC_DIRECTORY_STATUS /* 212 */,
    RC_FILE_STATUS /*213*/,
    RC_HELP /*214*/,
    RC_NAME /*215*/,
    RC_READY /*220*/,
    RC_BYE /*221*/,
    RC_DATA_CONNECTION_OPEN /*225*/,
    RC_CLOSING_DATA_CONNECTION /*226*/,
    RC_PASSIVE /*227*/,
    RC_LOGGED_IN /*230*/,
    RC_SECURITY_DATA_EXCHANGE_COMPLETE, /*234*/
    RC_FILE_ACTION_OK /*250*/,
    RC_PATHNAME_CREATED /*257*/,
    RC_NEED_PASSWD /*331*/,
    RC_FILE_ACTION_PENDING_INFO /*350*/,
    RC_SERVICE_NOT_AVAILABLE /*421*/, // shutting down
    RC_CANT_OPEN_DATA_CONN /*425*/,
    RC_TRANSFER_ABORTED /*426*/,
    RC_ACTION_FAILED /*450*/,
    RC_LOCAL_ERROR /*451*/,
    RC_OUT_OF_DISKSPACE /*452*/,
    RC_SYNTAX_ERROR /*500*/,
    RC_SYNTAX_ERROR_IN_PARAMS /*501*/,
    RC_COMMAND_NOT_IMPLEMENTED /*502*/,
    RC_BAD_SEQUENCE_OF_COMMANDS /*503*/,
    RC_PARAM_NOT_IMPLEMENTED /*504*/,
    RC_NOT_LOGGED_ON /*530*/,
    RC_REJECTED_FOR_POLICY_REASONS, /* 534 */
    RC_ACTION_NOT_TAKEN /*550*/,
    RC_ILLEGAL_FILE_NAME /*553*/
};

const std::pair<int, std::string>& Resolve(const FtpReplyCodes& code);

/*! Simple class for FTP replies */
class FtpReply
{
public:
    typedef std::shared_ptr<FtpReply> ptr_t;

    FtpReply() = default;

    std::ostringstream& Reply(FtpReplyCodes code, const bool endSession = false)
    {
        WAR_ASSERT(!has_replied_ && message_.str().empty() && "We already have replied!");
        code_ = code;
        do_end_session_ = endSession;
        have_reply_ = true;
        return message_;
    }

    std::ostringstream& VerboseReply(FtpReplyCodes code)
    {
        WAR_ASSERT(!has_replied_ && message_.str().empty() && "We already have replied!");
        code_ = code;
        have_reply_ = true;
        multi_line_ = true;
        return message_;
    }


    std::pair<FtpReplyCodes, const std::string> GetReply() {
        has_replied_ = true;
        return std::make_pair(code_, message_.str());
    }

    bool HasReplied() const { return has_replied_; }

    bool NeedToEndSession() const { return do_end_session_; }

    bool HaveReply() const { return have_reply_; }

    bool IsMultiline() const { return multi_line_; }

private:
    std::ostringstream message_;
    bool has_replied_ = false;
    FtpReplyCodes code_ = FtpReplyCodes::RC_SERVICE_NOT_AVAILABLE;
    bool do_end_session_ = false;
    bool have_reply_ = false;
    bool multi_line_ = false;
};


class MdtxState
{
public:
    enum class Facts {
        SIZE,
        MODIFY,
        TYPE,
        UNIQUE,
        PERM,
    };

    /*! Return a bitmap representing the Facts
     *
     * Each enabled Fact has it's bit set to 1.
     *
     * The bits corresponds to the integer value of Facts, so that
     * Facts::SIZE is the first bit == 1 << 0;
     */
    uint32_t GetEnabledFacts() const noexcept { return fact_bits_; }

    const std::string& GetFactName(const Facts& fact) const noexcept {
        return fact_names_[static_cast<int>(fact)];
    }

    bool IsEnabled(const Facts fact) const noexcept {
        return fact_bits_ & (1 << static_cast<int>(fact));
    }

    void Enable(const std::string& name) {
        for(const auto& f : facts_) {
            if (boost::iequals(GetFactName(f), name)) {
                fact_bits_ |= (1 << static_cast<int>(f));
            }
        }
    }

    std::ostream& GetFactList(std::ostream& o) const {
        for(const auto f : facts_) {
            o << GetFactName(f);

            if (IsEnabled(f))
                o << '*';

            o << ';';
        }

        return o;
    };

    const auto& GetFacts() const noexcept { return facts_; }

    void DisableAll() noexcept { fact_bits_ = 0; }

private:
    uint32_t fact_bits_ = 0b0000000000010111;
    static const std::vector<std::string> fact_names_;
    static const std::vector<Facts> facts_;
};

/*! Definition and naive default implementation of a PASV acceptor
 */
class PasvAcceptor
{
public:
    PasvAcceptor() {};
    PasvAcceptor(const PasvAcceptor&) = delete;
    virtual ~PasvAcceptor() {};
    PasvAcceptor& operator = (const PasvAcceptor&) = delete;

    /*! Start to listen to the specified endpoint */
    virtual void Listen(const boost::asio::ip::tcp::endpoint& ep) = 0;

    /*! Get the address the acceptor is listening to */
    virtual boost::asio::ip::tcp::endpoint GetAddress() const = 0;

    virtual boost::asio::ip::tcp::acceptor& GetAcceptor() = 0;
};

/*! The FTP protocol is stateful. This class keeps the current state */
struct FtpState
{
    enum class Initiation { NONE, PORT, PASV };
    enum class Type { ASCII, BIN };
    enum Transfer { NONE, INCOMING, OUTGOING };

    bool IsInTransfer() const { return transfer != Transfer::NONE; }
    bool IsLoggedIn() const { return is_logged_in; }
    void SetPort(boost::asio::ip::tcp::endpoint endp) {
        port_endpoint = endp;
        initiation = Initiation::PORT;
    }
    void SetPasv() {
        initiation = Initiation::PASV;
    }

    bool HavePasvOrPost() const {
        return initiation != Initiation::NONE;
    }

    void Abort() { abort_pending = true; }

    boost::asio::ip::tcp::endpoint GetPortEndpoint() const {
        WAR_ASSERT(initiation == Initiation::PORT);
        return port_endpoint;
    }

    /*! Called after a transfer is done with */
    void ResetTransfer() {
        transfer = Transfer::NONE;
        initiation = Initiation::NONE;
        allo = rest = 0;
        override_as_binary_ = false;
        pasv.reset();
        list_hidden_files = false;
    }

    Type GetType() const noexcept {
        if (override_as_binary_)
            return Type::BIN;
        return ttype;
    }

    bool is_logged_in = false;

    Transfer transfer = Transfer::NONE;
    Initiation initiation = Initiation::NONE;
    Type ttype = Type::ASCII;
    std::string addr;
    std::string requested_path_;
    boost::asio::ip::tcp::endpoint port_endpoint;
    std::uint64_t rest = 0;
    std::uint64_t allo = 0;
    bool abort_pending = false;
    std::string rnfr_;
    MdtxState mdtx_state;
    bool override_as_binary_ = false;
    std::unique_ptr<PasvAcceptor> pasv;
    bool list_hidden_files = false;
    std::string login_name;
    bool cc_is_encrypted = false;
    bool encrypt_transfers = false;
    std::vector<task_t> tasks_pending_after_reply;

};

/*! Base class for FTP commands
 *
 * This class is not just an interface, it also contains a default
 * implementation, so that we can override just what we need in order
 * to implement a specific command.
 *
 * The FTP commands are shared between the sessions, and contains no
 * session-relevant state. The required state is passed to OnCmd as
 * an argument.
 */
class FtpCmd
{
public:
    using ftp_commands_t = std::unordered_map<std::string, FtpCmd&>;
    using match_t = boost::smatch;
    using param_t = boost::string_ref;

    FtpCmd(const std::string& name, const std::string help = "",
           const std::string& syntax = "", const std::string& feat = "")
    : regex_(syntax, boost::regex::icase), help_{help}, name_{name}
    , feat_{feat}
    {}

    virtual ~FtpCmd() {}

    /*! Get the name of the command. */
    const std::string& GetName() const { return name_; }

    /*! Get the help for the command. */
    const std::string& GetHelp() const { return help_; }

    const bool CanHaveParams() const {
        return regex_.size() > 0;
    }

    const std::string& GetFeat() const { return feat_; }

    /*! Get the regex for the command. */
    const boost::regex& GetRegex() const { return regex_; }

    /*! Returs true if the command requires the client to be logged on */
    virtual bool MustBeLoggedIn() const { return false; }

    /*!  true if the command requires the client NOT to be logged on */
    virtual bool MustNotBeLoggedIn() const { return false; }

    /*! Returs true if the command requires the client to be transferring a file */
    virtual bool MustBeInTransfer() const { return false; }

    /*! Returs true if the command requires the client to NOT to be transffering a file */
    virtual bool MustNotBeInTransfer() const { return false; }

    /*! Must be preceeded by a valid STOR or PASV conmand */
    virtual bool NeedPostOrPasv() const { return false; }

    /*! Can only be used after encryption is established if
     *  encryption (TLS) is mandentory.
     */
    virtual bool MustHaveEncryptionIfEnforced() const { return true; }

    /*! Can only be used after encryption is established */
    virtual bool MustHaveEncryption() const { return false; }

    /*! Returns the command required as the previos command.

        If an empty string re returned, this command does not require a
        specific previous command
    */
    virtual const std::string& NeedPrevCmd() const {
        static const std::string empty;
        return empty;
    }

    /*! Called when the command is requested from the client */
    virtual void OnCmd(Session& session,
                       FtpState& state,
                       const param_t& cmd,
                       const param_t& param,
                       const match_t& match,
                       FtpReply& reply) = 0;

    /*! Called when the opts command is received for this command */
    virtual void OnOpts(Session& session,
                        FtpState& state,
                        const param_t& cmd,
                        const param_t& param,
                        const match_t& match,
                        FtpReply& reply);

private:
    /// Regex for the command's parameters
    const boost::regex regex_;
    /// The help-string provided for this command
    const std::string help_;
    /// Name of the command
    const std::string name_;
    /// FEAT message
    const std::string feat_;
};


/*! Get the default implementation of the FTP commands */
FtpCmd::ftp_commands_t GetDefaultFtpCommands();

} // wfde
} // war

/*! Print the message corresponding to the reply-code
 *
 * The message is printed as nnn<sp>text, where nnn is the return-code
 * associated with the FTP reoply message.
 */
std::ostream& operator << (std::ostream& o, const war::wfde::FtpReplyCodes& code);
std::ostream& operator << (std::ostream& o, const war::wfde::FtpState::Type type);
