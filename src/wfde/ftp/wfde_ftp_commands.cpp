#include "war_wfde.h"

#include <vector>
#include <memory>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include "log/WarLog.h"
#include "wfde/ftp_protocol.h"
#include "war_uuid.h"
#include "war_helper.h"
#include "tasks/WarPipeline.h"

#include "WfdeFtpList.h"

using namespace std;
using namespace string_literals;


std::ostream& operator << (std::ostream& o, const war::wfde::FtpState::Type type)
{
    return o << (type == war::wfde::FtpState::Type::ASCII ? "ASCII"s : "Binary"s);
}

namespace war {
namespace wfde {

const vector<string> MdtxState::fact_names_ = {
    "Size", "Modify", "Type", "Unique", "Perm"
};

const vector<MdtxState::Facts> MdtxState::facts_ = {
    MdtxState::Facts::SIZE, MdtxState::Facts::MODIFY,
    MdtxState::Facts::TYPE, MdtxState::Facts::UNIQUE, MdtxState::Facts::PERM
};

void FtpCmd::OnOpts(Session& session,
                       FtpState& state,
                       const param_t& cmd,
                       const param_t& param,
                       const match_t& match,
                       FtpReply& reply)
{
    reply.Reply(FtpReplyCodes::RC_SYNTAX_ERROR_IN_PARAMS)
        << "This command has no options";
}

namespace impl {

class FtpCmdQuit : public FtpCmd
{
public:
    FtpCmdQuit() : FtpCmd("QUIT") {}

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        reply.Reply(FtpReplyCodes::RC_BYE, true);
    }
};

class FtpCmdAbor : public FtpCmd
{
public:
    FtpCmdAbor() : FtpCmd("ABOR") {}

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        state.Abort();
    }
};

class FtpCmdFeat : public FtpCmd
{
public:
    using feat_fun_t = std::function<string (const FtpState&)>;

    bool MustHaveEncryptionIfEnforced() const { return false; }

    FtpCmdFeat() : FtpCmd("FEAT") {}

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        if (!features_.empty()) {
            std::ostringstream features;

            for(const auto& f : features_) {
                features << "\r\n " << (f.second ? f.second(state) : f.first);
            }

            reply.VerboseReply(FtpReplyCodes::RC_STATUS_OK)
                << "Extensions supported:"
                << features.str();
        } else {
            reply.Reply(FtpReplyCodes::RC_STATUS_OK) << "No Features installed";
        }

    }

    void AddFeature(const string& name, feat_fun_t f = nullptr) {
        features_[name] = f;
    }

private:
    std::map<string, feat_fun_t> features_ = {
        {"TVFS", nullptr},
        {"MLST", [](const FtpState& state) {
            stringstream out;
            out << "MLST ";
            state.mdtx_state.GetFactList(out);
            return out.str();
        }}
    };
};

class FtpCmdOpts : public FtpCmd
{
public:
    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    FtpCmdOpts() : FtpCmd("OPTS", "[PARAM [PARAM...]]", "([a-z]+)(\\ (.+))?") {}

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        string name(match[1].first, match[1].second);
        const string options(match[3].first, match[3].second);

        boost::to_upper(name);
        auto cmd_it = cmds_.find(name);
        if (cmd_it != cmds_.end()) {
            cmd_it->second->OnOpts(session, state, name, options, match, reply);
            return;
        }

        // Use the default implemented error-message
        OnOpts(session, state, cmd, param, match, reply);
    }

    void AddFeature(FtpCmd& cmd) {
        cmds_[cmd.GetName()] = &cmd;
    }

private:
    std::map<string, FtpCmd *> cmds_;
};


class FtpCmdSyst : public FtpCmd
{
public:
    FtpCmdSyst() : FtpCmd("SYST") {}

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        reply.Reply(FtpReplyCodes::RC_NAME) << "UNIX Type: L8";
    }
};


class FtpCmdType : public FtpCmd
{
public:
    FtpCmdType() : FtpCmd("TYPE", "A[ N]|I", "([A](\\ N)?)|(I)") {}

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(!param.empty());
        std::locale loc;
        if ('I' == std::toupper(param[0], loc)) {
            state.ttype = FtpState::Type::BIN;
        } else {
            state.ttype = FtpState::Type::ASCII;
        }
        reply.Reply(FtpReplyCodes::RC_OK) << state.ttype << " type OK";
    }
};

class FtpCmdUser : public FtpCmd
{
public:
    FtpCmdUser() : FtpCmd("USER", "username", "(.+)") {}

    bool MustNotBeLoggedIn() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        state.login_name = param.to_string();

        if (session.AuthenticateWithPasswd(state.login_name, ""s)) {
            state.is_logged_in = true;
            reply.Reply(FtpReplyCodes::RC_LOGGED_IN);
        } else {
            reply.Reply(FtpReplyCodes::RC_NEED_PASSWD);
        }
    }
};

class FtpCmdPass : public FtpCmd
{
public:
    FtpCmdPass() : FtpCmd("PASS", "password", "(.+)") {}

    bool MustNotBeLoggedIn() const override { return true; }

    const string& NeedPrevCmd() const override {
        static const string user("USER");
        return user;
    }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {

        const std::string pwd(param);

        if (session.AuthenticateWithPasswd(state.login_name, pwd)) {
            state.is_logged_in = true;
            reply.Reply(FtpReplyCodes::RC_LOGGED_IN);
        } else {
            reply.Reply(FtpReplyCodes::RC_NOT_LOGGED_ON);
        }
    }
};

class FtpCmdPort : public FtpCmd
{
public:
    FtpCmdPort() : FtpCmd("PORT", "h1,h2,h3,h4,p1,p2",
                          R"((\d{1,3}),(\d{1,3}),(\d{1,3}),(\d{1,3}),(\d{1,3}),(\d{1,3}))")
    {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        // Somehow, lexical_cast for unsigned char fails if the value
        // triggers the 8th bit.
        const unsigned char p1 = (unsigned char)boost::lexical_cast<
            unsigned short>(string(match[5].first, match[5].second));
        const unsigned char p2 = (unsigned char)boost::lexical_cast<
            unsigned short>(string(match[6].first, match[6].second));

        const unsigned short port = (p1 << 8) | p2;

        const string ip_str = string(match[1].first, match[1].second) + "."
            + string(match[2].first, match[2].second) + "."
            + string(match[3].first, match[3].second) + "."
            + string(match[4].first, match[4].second);

        auto ip = boost::asio::ip::address::from_string(ip_str);

        state.SetPort({ip, port});
        reply.Reply(FtpReplyCodes::RC_OK);
    }
};

class PasvAcceptorImpl : public PasvAcceptor
{
public:
    PasvAcceptorImpl(Pipeline& pipeline)
    : acceptor_{pipeline.GetIoService()}
    {
    }

    ~PasvAcceptorImpl() {}

    void Listen(const boost::asio::ip::tcp::endpoint& ep) override
    {
        acceptor_.open(ep.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(ep);
        acceptor_.listen();
    }

    boost::asio::ip::tcp::endpoint GetAddress() const override {
        return acceptor_.local_endpoint();
    }

    boost::asio::ip::tcp::acceptor& GetAcceptor() override {
        return acceptor_;
    }

private:
    boost::asio::ip::tcp::acceptor acceptor_;
};

class FtpCmdPasv : public FtpCmd
{
public:
    FtpCmdPasv() : FtpCmd("PASV")
    {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        if (!state.pasv) {
            state.pasv = make_unique<PasvAcceptorImpl>(
                session.GetSocket().GetPipeline());

            boost::asio::ip::tcp::endpoint ep
                {session.GetSocket().GetSocket().local_endpoint().address(), 0};

            state.pasv->Listen(ep);
        }

        const auto lep = state.pasv->GetAddress();
        const auto ip = lep.address().to_v4();

        reply.Reply(FtpReplyCodes::RC_PASSIVE) << "Entering Passive Mode ("
            << to_string(ip.to_bytes()[0]) << ','
            << to_string(ip.to_bytes()[1]) << ','
            << to_string(ip.to_bytes()[2]) << ','
            << to_string(ip.to_bytes()[3]) << ','
            << ((lep.port() >> 8) & 0xff) << ','
            << (lep.port() & 0xff) << ')';

        LOG_DEBUG_FN << "Listening on " << state.pasv->GetAddress()
            << ' ' << session;

        state.SetPasv();
    }
};

class FtpCmdRetr : public FtpCmd
{
public:
    FtpCmdRetr() : FtpCmd("RETR", "pathname", "(.+)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool NeedPostOrPasv() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(param.size() > 0);
        state.requested_path_.assign(param.begin(), param.end());

        if (state.rest && (state.GetType() != FtpState::Type::BIN)) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "REST is only available for binary (IMAGE) type transfers";
            return;
        }

        try {
            auto file = session.OpenFile(state.requested_path_,
                                         File::FileOperation::READ);
            WAR_ASSERT(file != nullptr);
            session.GetSessionData().StartTransfer(move(file));
        } catch (ExceptionFailedToConnect&) {
            reply.Reply(FtpReplyCodes::RC_CANT_OPEN_DATA_CONN);
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_FAILED);
        );
    }
};

class FtpCmdStor : public FtpCmd
{
public:
    FtpCmdStor() : FtpCmd("STOR", "pathname", "(.+)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool NeedPostOrPasv() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(param.size() > 0);
        state.requested_path_.assign(param.begin(), param.end());

        if (state.rest && (state.GetType() != FtpState::Type::BIN)) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "REST is only available for binary (IMAGE) type transfers";
            return;
        }

        try {
            auto file = session.OpenFile(state.requested_path_,
                                         File::FileOperation::WRITE);
            session.GetSessionData().StartTransfer(move(file));
        } catch (ExceptionFailedToConnect&) {
            reply.Reply(FtpReplyCodes::RC_CANT_OPEN_DATA_CONN);
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_FAILED);
        );
    }
};

class FtpCmdStou : public FtpCmd
{
public:
    FtpCmdStou() : FtpCmd("STOU") {}

    bool MustBeLoggedIn() const override { return true; }
    bool NeedPostOrPasv() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {

        state.requested_path_ = get_uuid_as_string();

        if (state.rest) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "STOU can not be combined with REST";
            return;
        }

        try {
            auto file = session.OpenFile(state.requested_path_,
                                         File::FileOperation::WRITE_NEW);
            session.GetSessionData().StartTransfer(move(file));
        } catch (ExceptionFailedToConnect&) {
            reply.Reply(FtpReplyCodes::RC_CANT_OPEN_DATA_CONN);
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_FAILED);
        );
    }
};

class FtpCmdAppe : public FtpCmd
{
public:
    FtpCmdAppe() : FtpCmd("APPE", "pathname", "(.+)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool NeedPostOrPasv() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(param.size() > 0);
        state.requested_path_.assign(param.begin(), param.end());

        if (state.rest) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "APPE can not be combined with REST";
            return;
        }

        try {
            auto file = session.OpenFile(state.requested_path_,
                                         File::FileOperation::APPEND);
            session.GetSessionData().StartTransfer(move(file));
        } catch (ExceptionFailedToConnect&) {
            reply.Reply(FtpReplyCodes::RC_CANT_OPEN_DATA_CONN);
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_FAILED);
        );
    }
};

class FtpCmdRest : public FtpCmd
{
public:
    FtpCmdRest() : FtpCmd("REST", "file-offset", R"((\d+))", "REST STREAM") {}

    bool MustBeLoggedIn() const override { return true; }
    bool NeedPostOrPasv() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(param.size() > 0);

        state.rest = 0;
        auto rest_val = state.rest;

        try {
            rest_val = boost::lexical_cast<decltype(state.rest)>(param);
        } catch(const boost::bad_lexical_cast&) {
            reply.Reply(FtpReplyCodes::RC_SYNTAX_ERROR_IN_PARAMS);
            return;
        }

        /* For now, we deny rest offsets into ascii-type files, as
         * they are likely to be used for DoS attacks, forcing the server
         * to read and parse huge files to find the ASCII location
         * for a large file offset.
         */
        if (rest_val && (state.GetType() != FtpState::Type::BIN)) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "REST is only available for binary (IMAGE) type transfers";
            return;
        }

        state.rest = rest_val;

        reply.Reply(FtpReplyCodes::RC_FILE_ACTION_PENDING_INFO)
            << "Restarting at " << rest_val << ". Send STOR or RETR.";
    }
};

class FtpCmdSize : public FtpCmd
{
public:
    FtpCmdSize() : FtpCmd("SIZE", "pathname", "(.+)", "SIZE") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(param.size() > 0);
        state.requested_path_.assign(param.begin(), param.end());

        /* For now, we deny size queries for ascii-type files, as
         * they are likely to be used for DoS attacks, forcing the server
         * to read and parse huge files to find the ASCII size.
         */
        if (state.GetType() != FtpState::Type::BIN) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "SIZE is only available while in binary "
                << "(IMAGE) type transfer mode";
            return;
        }

        try {
            auto size = session.GetFileLen(state.requested_path_);
            reply.Reply(FtpReplyCodes::RC_FILE_STATUS) << size;
            return;
        } WAR_CATCH_ERROR;

        reply.Reply(FtpReplyCodes::RC_ACTION_FAILED);
    }
};

class FtpCmdMdtm : public FtpCmd
{
public:
    FtpCmdMdtm() : FtpCmd("MDTM", "pathname", "(.+)", "MDTM") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(param.size() > 0);
        const string vpath(param.begin(), param.end());

        state.requested_path_.assign(param.begin(), param.end());
        auto path = session.GetPath(vpath, Path::Type::FILE);

        try {
            if (path->Exists()) {
                reply.Reply(FtpReplyCodes::RC_FILE_STATUS)
                    << GetRfc3659FileTime(*path);
                return;
            }
        } WAR_CATCH_ERROR;

        reply.Reply(FtpReplyCodes::RC_ACTION_FAILED);
    }
};

// TODO: Verify and eventually reserve the requested storage space
class FtpCmdAllo : public FtpCmd
{
public:
    FtpCmdAllo() : FtpCmd("ALLO", "bytes", R"((\d+))") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(param.size() > 0);

        try {
            state.allo = boost::lexical_cast<decltype(state.allo)>(param);
        } catch(const boost::bad_lexical_cast&) {
            reply.Reply(FtpReplyCodes::RC_SYNTAX_ERROR_IN_PARAMS);
            return;
        }

        reply.Reply(FtpReplyCodes::RC_OK);
    }
};

class FtpCmdPwd : public FtpCmd
{
public:
    FtpCmdPwd() : FtpCmd("PWD") {}

    bool MustBeLoggedIn() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        reply.Reply(FtpReplyCodes::RC_PATHNAME_CREATED)
            << session.GetCwd();
    }
};

class FtpCmdCwd : public FtpCmd
{
public:
    FtpCmdCwd() : FtpCmd("CWD", "path", "(.*)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        Path::vpath_t path{param.begin(), param.end()};

        if (path.empty()) {
            //TODO: Go to the home directory
            path = "/";
        }

        try {
            session.SetCwd(path);
            reply.Reply(FtpReplyCodes::RC_FILE_ACTION_OK)
                << "Directory changed to " << session.GetCwd();
        } catch (const ExceptionAccessDenied&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
        } catch (const ExceptionBadPath&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "Internal Server Error";
        );
    }
};

class FtpCmdCdup : public FtpCmd
{
public:
    FtpCmdCdup() : FtpCmd("CDUP") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        try {
            session.SetCwd("..");
            reply.Reply(FtpReplyCodes::RC_FILE_ACTION_OK)
                << "Directory changed to " << session.GetCwd();
        } catch (const ExceptionAccessDenied&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
        } catch (const ExceptionBadPath&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "Internal Server Error";
        );
    }
};

class FtpCmdDele : public FtpCmd
{
public:
    FtpCmdDele() : FtpCmd("DELE", "path", "(.+)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(param.size() > 0);

        const Path::vpath_t path{param.begin(), param.end()};

        try {
            session.DeleteFile(path);
            reply.Reply(FtpReplyCodes::RC_FILE_ACTION_OK);
        } catch (const ExceptionAccessDenied&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
        } catch (const ExceptionBadPath&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
         } catch (const ExceptionNotFound&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "No such file";
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "Internal Server Error";
        );
    }
};

class FtpCmdMkd : public FtpCmd
{
public:
    FtpCmdMkd() : FtpCmd("MKD", "path", "(.+)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(param.size() > 0);

        const Path::vpath_t path{param.begin(), param.end()};

        try {
            session.CreateDirectory(path);
            reply.Reply(FtpReplyCodes::RC_FILE_ACTION_OK);
        } catch (const ExceptionAccessDenied&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
        } catch (const ExceptionBadPath&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
         } catch (const ExceptionNotFound&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "No such file";
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "Internal Server Error";
        );
    }
};

class FtpCmdRmd : public FtpCmd
{
public:
    FtpCmdRmd() : FtpCmd("RMD", "path", "(.+)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(param.size() > 0);

        const Path::vpath_t path{param.begin(), param.end()};

        try {
            session.DeleteDirectory(path);
            reply.Reply(FtpReplyCodes::RC_FILE_ACTION_OK);
        } catch (const ExceptionAccessDenied&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
        } catch (const ExceptionBadPath&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
        } catch (const ExceptionAlreadyExist&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Cannot create an exitsing directory.";
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "Internal Server Error";
        );
    }
};

class FtpCmdNoop : public FtpCmd
{
public:
    FtpCmdNoop() : FtpCmd("NOOP") {}

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        reply.Reply(FtpReplyCodes::RC_OK);
    }
};


class FtpCmdRnfr : public FtpCmd
{
public:
    FtpCmdRnfr() : FtpCmd("RNFR", "path", "(.+)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(param.size() > 0);

        const Path::vpath_t path{param.begin(), param.end()};

        try {
            auto from_path = session.GetPath(path, Path::Type::ANY);

            if (!from_path->CanRename()) {
                LOG_DEBUG_FN << "The client does not have CAN_RENAME access to "
                    << log::Esc(from_path->GetVirtualPath());
                reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
            } else if (!from_path->Exists()) {
                LOG_DEBUG_FN << "The file or directory "
                    << log::Esc(from_path->GetPhysPath().string())
                    << " does not exist.";
                reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "No such file or directory";
            } else {
                reply.Reply(FtpReplyCodes::RC_FILE_ACTION_PENDING_INFO);
                state.rnfr_ = from_path->GetVirtualPath();
            }
        } catch (const ExceptionBadPath&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "Internal Server Error";
        );
    }
};

class FtpCmdRnto : public FtpCmd
{
public:
    FtpCmdRnto() : FtpCmd("RNTO", "path", "(.+)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }
    const string& NeedPrevCmd() const override {
        static const string rnfr("RNFR");
        return rnfr;
    }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        WAR_ASSERT(param.size() > 0);

        const Path::vpath_t path{param.begin(), param.end()};

        try {
            session.Rename(state.rnfr_, path);
            reply.Reply(FtpReplyCodes::RC_FILE_ACTION_OK);
        } catch (const ExceptionAccessDenied&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
        } catch (const ExceptionBadPath&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN) << "Access denied";
        } catch (const ExceptionAlreadyExist&) {
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "Cannot create an exitsing directory.";
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_NOT_TAKEN)
                << "Internal Server Error";
        );
    }
};

class FtpCmdHelp : public FtpCmd
{
public:
    FtpCmdHelp() : FtpCmd("HELP", "[command]", "(.*)")
    {
        AddCmd(*this);
    }

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        if (param.size()) {
            string command{param.begin(), param.end()};
            boost::to_upper(command);

            auto cmd = cmds_.find(command);
            if (cmd == cmds_.end()) {
                reply.Reply(FtpReplyCodes::RC_SYNTAX_ERROR_IN_PARAMS)
                    << "Command '" << command  << "' not found.";
            } else {
                reply.Reply(FtpReplyCodes::RC_HELP)
                    << cmd->second->GetName() << ' ' << cmd->second->GetHelp();
            }
        } else {
            std::ostringstream help;
            help << "Available commands:";
            help.fill(' ');
            auto col = 0;
            static const string crlf("\r\n");
            for(const auto& cmd: cmds_) {
                if (!(col++ % 8))
                    help << crlf;
                help << std::setw(6) << std::left << cmd.first;
            }
            reply.VerboseReply(FtpReplyCodes::RC_HELP) << help.str();
        }
    }

    void AddCmd(FtpCmd& cmd) {
        cmds_[cmd.GetName()] = &cmd;
    }

    // Call if you make changes and remove a command
    // If you replace, call AddCmd() on the new command.
    void RemoveCmd(const string& name) {
        cmds_.erase(name);
    }

private:
    std::map<string, FtpCmd *> cmds_;
};


namespace {
    enum class Format { LONG, SHORT };
    string ParseListArgs(const FtpCmd::param_t& param, FtpState& state,
                              Format& format) {
        // Simple command-line parser (since boost::program_options lack a
        // string2argv parser like split_winmain() under linux).
        // TODO: Implement combined form, ie. -la
        string vpath;
        for(auto p = param.begin(); p != param.end(); ++p) {
            // Start of segment
            if (*p == '-') {
                auto pp = p;
                if (++pp != param.end()) {
                    if (*pp == '1') { // one
                        format = Format::SHORT;
                    } else if (*pp == 'l') { // el
                        format = Format::LONG;
                    } else if (*pp == 'a') {
                        state.list_hidden_files = true;
                    } else {
                        goto get_path;
                    }
                     // Skip approved command-letter and space(s)
                    for(p += 2; (p != param.end()) && (*p == ' '); ++p)
                        ; //loop

                    --p; // We will increment again in the outer loop.
                }
            } else {
get_path:
                vpath.assign(p, param.end());
                break;
            }
        }

        return vpath;
    }
}

class FtpCmdList : public FtpCmd
{
public:
    FtpCmdList() : FtpCmd("LIST", "[path]", "(.*)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }
    bool NeedPostOrPasv() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        Format format = Format::LONG;
        string vpath = ParseListArgs(param, state, format);

        try {
            auto path = session.GetPathForListing(vpath);

            std::unique_ptr<File> file;

            if (format == Format::LONG) {
                file = make_unique<WfdeFtpList<DirListerLs>>(move(path),
                                                             session,
                                                             state);
            } else {
                // Format:::SHORT
                file = make_unique<WfdeFtpList<DirListerNlst>>(move(path),
                                                               session,
                                                               state);
            }
            WAR_ASSERT(file != nullptr);

            session.GetSessionData().StartTransfer(move(file));
        } catch (ExceptionFailedToConnect&) {
            reply.Reply(FtpReplyCodes::RC_CANT_OPEN_DATA_CONN);
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_FAILED);
        );
    }
};

class FtpCmdNlst : public FtpCmd
{
public:
    FtpCmdNlst() : FtpCmd("NLST", "[path]", "(.*)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }
    bool NeedPostOrPasv() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        try {
            auto path = session.GetPathForListing(param.to_string());

            auto file = make_unique<WfdeFtpList<DirListerNlst>>(move(path),
                                                              session,
                                                              state);
            WAR_ASSERT(file != nullptr);

            session.GetSessionData().StartTransfer(move(file));
        } catch (ExceptionFailedToConnect&) {
            reply.Reply(FtpReplyCodes::RC_CANT_OPEN_DATA_CONN);
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_FAILED);
        );
    }
};

class FtpCmdMlsd : public FtpCmd
{
public:
    FtpCmdMlsd() : FtpCmd("MLSD", "[path]", "(.*)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }
    bool NeedPostOrPasv() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        string target_path;
        if (param.size()) {
            target_path.assign(param.begin(), param.end());
        }

        try {
            auto path = session.GetPathForListing(target_path);
            auto file = make_unique<WfdeFtpList<DirListerMlsd>>(move(path),
                                                                session,
                                                                state);
            WAR_ASSERT(file != nullptr);

            state.override_as_binary_ = true;
            session.GetSessionData().StartTransfer(move(file));
        } catch (ExceptionFailedToConnect&) {
            reply.Reply(FtpReplyCodes::RC_CANT_OPEN_DATA_CONN);
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_FAILED);
        );
    }
};

class FtpCmdMlst : public FtpCmd
{
public:
    FtpCmdMlst() : FtpCmd("MLST", "[path]", "(.*)") {}

    bool MustBeLoggedIn() const override { return true; }
    bool MustNotBeInTransfer() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        string vpath;
        if (param.size()) {
            vpath.assign(param.begin(), param.end());
        }

        try {
            auto path = session.GetPath(vpath, Path::Type::ANY);
            if (!path->Exists()) {
                LOG_DEBUG_FN << "Failed to get the vpath "
                    << log::Esc(vpath);
                WAR_THROW_T(ExceptionNotFound, vpath);
            }

            if (!path->CanList()) {
                LOG_DEBUG_FN << "Missing CAN_LIST for vpath "
                    << log::Esc(vpath);
                WAR_THROW_T(ExceptionAccessDenied, vpath);
            }

            string data = GetMlstFacts(*path, session, state);

            reply.VerboseReply(FtpReplyCodes::RC_FILE_ACTION_OK)
                << "Listing " << vpath << "\r\n" << data;

        } catch (ExceptionFailedToConnect&) {
            reply.Reply(FtpReplyCodes::RC_CANT_OPEN_DATA_CONN);
        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_FAILED);
        );
    }

    void OnOpts(Session& session,
                FtpState& state,
                const param_t& cmd,
                const param_t& param,
                const match_t& match,
                FtpReply& reply) override
    {
        LOG_DEBUG_FN << "Options for command " << cmd << " has param "
            << log::Esc(param);

        {
            std::vector<string> facts;
            boost::split(facts, param, boost::is_any_of("; "));

            state.mdtx_state.DisableAll();
            for(const auto name : facts) {
                state.mdtx_state.Enable(name);
            }
        }

        std::ostringstream facts_out;

        for(const auto& f : state.mdtx_state.GetFacts()) {
            if (state.mdtx_state.IsEnabled(f)) {
                facts_out << state.mdtx_state.GetFactName(f) << ';';
            }
        }

        reply.Reply(FtpReplyCodes::RC_OK) << facts_out.str();
    }
};


class FtpCmdStat : public FtpCmd
{
public:
    FtpCmdStat() : FtpCmd("STAT", "[path]", "(.*)") {}

    bool MustBeLoggedIn() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        if (param.empty()) {
            Stat(state, reply);
        } else {
            List(session, state, param, reply);
        }
    }

private:
    void List(Session& session, FtpState& state, const param_t& param,
              FtpReply& reply)
    {
        Format format = Format::LONG;
        string vpath = ParseListArgs(param, state, format);

        try {
            auto path = session.GetPathForListing(vpath);

            std::unique_ptr<File> file;

            if (format == Format::LONG) {
                file = make_unique<WfdeFtpList<DirListerLs>>(move(path),
                                                             session,
                                                             state);
            } else {
                // Format:::SHORT
                file = make_unique<WfdeFtpList<DirListerNlst>>(move(path),
                                                               session,
                                                               state);
            }
            WAR_ASSERT(file != nullptr);

            // Here we may consume a little too much memory on large listings
            string listing = " ";
            while(!file->IsEof()) {
                auto b = file->Read();
                auto p = boost::asio::buffer_cast<const char *>(b);
                const auto end = p + boost::asio::buffer_size(b);

                for(; p != end; ++p) {
                    listing += *p;
                    if (*p == '\n') {
                        listing += ' ';
                    }
                }

                // TODO: Async flush the output buffer here
            }

            if (listing.size() >= 3) {
                listing.resize(listing.size() - 3); // Strip off trailing CRLF
            } else
                listing.clear();

            reply.VerboseReply(FtpReplyCodes::RC_DIRECTORY_STATUS)
                << param << "\r\n"
                << listing;

        } WAR_CATCH_ALL_EF (
            reply.Reply(FtpReplyCodes::RC_ACTION_FAILED);
        );
    }
    void Stat(const FtpState& state, FtpReply& reply)
    {
        reply.Reply(FtpReplyCodes::RC_STATUS_OK)
            << (state.IsInTransfer()
                ? ("In transfer of file \""s + state.requested_path_ + "\", "s)
                : "Not in transfer, "s)
            << (state.HavePasvOrPost() ? "Awaiting transfer command, " : "")
            << (state.abort_pending ? "Abort pending, " : "")
            << "Structure=File, "
            << "Type=" << state.GetType() << ", "
            << "Rest offset=" << state.rest
            << ", CC-TLS=" << state.cc_is_encrypted
            << ", PROT=" << (state.encrypt_transfers ? "P" : "C")
            ;
    }
};

class FtpCmdAuth : public FtpCmd
{
public:
    FtpCmdAuth() : FtpCmd("AUTH", "tls", "(tls|tls-c)", "AUTH TLS") {}

    bool MustHaveEncryptionIfEnforced() const override { return false; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        if (state.cc_is_encrypted) {
            reply.Reply(FtpReplyCodes::RC_REJECTED_FOR_POLICY_REASONS)
                << "The control connection is already encrypted";
            return;
        }

        reply.Reply(FtpReplyCodes::RC_SECURITY_DATA_EXCHANGE_COMPLETE);
        state.tasks_pending_after_reply.push_back({
            [&session, &state] {
                try {
                    session.GetSessionData().StartTls();
                } WAR_CATCH_ERROR;
            }, "Switch to TLS on CC"});
    }
};

class FtpCmdPbsz : public FtpCmd
{
public:
    FtpCmdPbsz() : FtpCmd("PBSZ", "0", "(0)", "PBSZ") {}

    bool MustHaveEncryption() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        /* This command is mandated by RFC 2228 after AUTH, but for TLS
         * it has no function.
         *
         * So we do nothing except, for replying.
         */
        reply.Reply(FtpReplyCodes::RC_OK);
    }
};

class FtpCmdProt : public FtpCmd
{
public:
    FtpCmdProt() : FtpCmd("PROT", "protection-mode", "(P|C)", "PROT") {}

    bool MustHaveEncryption() const override { return true; }

    void OnCmd(Session& session,
               FtpState& state,
               const param_t& cmd,
               const param_t& param,
               const match_t& match,
               FtpReply& reply) override
    {
        state.encrypt_transfers = boost::iequals(param, "p"s);
        LOG_TRACE1_FN << "Will "
            << (state.encrypt_transfers ? "" : "NOT ")
            << " encrypt transfers from now on.";

        reply.Reply(FtpReplyCodes::RC_OK);
    }
};

class DefaultFtpCmds
{
public:
    DefaultFtpCmds()
    {
        // Add instances of the default FTP commands

        // Must be added first
        Add(make_unique<FtpCmdHelp>());

        // Must be added before any optional FEAT commands
        Add(make_unique<FtpCmdFeat>());
        Add(make_unique<FtpCmdOpts>());

        // Regular commands in any order
        Add(make_unique<FtpCmdQuit>());
        Add(make_unique<FtpCmdSyst>());
        Add(make_unique<FtpCmdType>());
        Add(make_unique<FtpCmdPort>());
        Add(make_unique<FtpCmdUser>());
        Add(make_unique<FtpCmdPass>());
        Add(make_unique<FtpCmdRetr>());
        Add(make_unique<FtpCmdStor>());
        Add(make_unique<FtpCmdStou>());
        Add(make_unique<FtpCmdAppe>());
        Add(make_unique<FtpCmdRest>());
        Add(make_unique<FtpCmdSize>());
        Add(make_unique<FtpCmdMdtm>());
        Add(make_unique<FtpCmdAllo>());
        Add(make_unique<FtpCmdAbor>());
        Add(make_unique<FtpCmdPwd>());
        Add(make_unique<FtpCmdCwd>());
        Add(make_unique<FtpCmdCdup>());
        Add(make_unique<FtpCmdDele>());
        Add(make_unique<FtpCmdMkd>());
        Add(make_unique<FtpCmdRmd>());
        Add(make_unique<FtpCmdNoop>());
        Add(make_unique<FtpCmdRnfr>());
        Add(make_unique<FtpCmdRnto>());
        Add(make_unique<FtpCmdList>());
        Add(make_unique<FtpCmdNlst>());
        Add(make_unique<FtpCmdMlsd>());
        Add(make_unique<FtpCmdMlst>());
        Add(make_unique<FtpCmdPasv>());
        Add(make_unique<FtpCmdStat>());
        Add(make_unique<FtpCmdAuth>());
        Add(make_unique<FtpCmdPbsz>());
        Add(make_unique<FtpCmdProt>());

        // Insert all the added commands into the default list that we hand
        // out to session instances.
        for(auto& cmd : commands_) {
            commands_list_.emplace(cmd->GetName(), *cmd);
        }
    }

    FtpCmd::ftp_commands_t GetDefaultCommands() {
        return commands_list_;
    }

private:
    void Add(std::unique_ptr<FtpCmd>&& cmd) {

        if (feat_ && !cmd->GetFeat().empty()) {
            feat_->AddFeature(cmd->GetFeat());
        }

        if (help_) {
            help_->AddCmd(*cmd);
        }

        if (opts_) {
            opts_->AddFeature(*cmd);
        }

        commands_.push_back(move(cmd));
    }

    void Add(std::unique_ptr<FtpCmdFeat>&& feat) {
        feat_ = feat.get();
        commands_.push_back(move(feat));
    }

    void Add(std::unique_ptr<FtpCmdHelp>&& help) {
        help_ = help.get();
        commands_.push_back(move(help));
    }

    void Add(std::unique_ptr<FtpCmdOpts>&& opts) {
        opts_ = opts.get();
        commands_.push_back(move(opts));
    }

    std::vector<std::unique_ptr<FtpCmd>> commands_;
    FtpCmdFeat *feat_ = nullptr;
    FtpCmdHelp *help_ = nullptr;
    FtpCmdOpts *opts_ = nullptr;
    FtpCmd::ftp_commands_t commands_list_;
};


} // impl

FtpCmd::ftp_commands_t GetDefaultFtpCommands()
{
    static impl::DefaultFtpCmds cmds_;
    return cmds_.GetDefaultCommands();
}

} // wfde
} //war
