/* Simple implementation of an authentication manager and it's Client object.
 *
 * This is used by wfded, but is not part of the wfde library itelf,
 * as derived FTP implementations are likely to have their own
 * requirements on user database format or use of external authentication
 * sources like pam or LDAP/KERBEROS.
 *
 */

#include <atomic>
#include <string>
#include <cstring>
#include <boost/algorithm/string/case_conv.hpp>

#include <wfde/wfde.h>
#include <warlib/impl.h>
#include <warlib/error_handling.h>
#include <warlib/WarLog.h>
#include <warlib/uuid.h>

namespace war {
namespace wfde {
namespace wfded {

// recommended in Meyers, Effective STL when internationalization and embedded
// NULLs aren't an issue.  Much faster than the STL or Boost lex versions.
struct ciLessLibC {
    bool operator()(const std::string &lhs, const std::string &rhs) const {
        return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
    }
};

class AuthManagerImpl;

class ClientImpl : public Client
{
public:
    ClientImpl(const std::string& name, const std::string& pwd,
               const Permissions::ptr_t& permissions)
    : name_{name}, pwd_{pwd}, id_(boost::uuids::random_generator()())
    , permissions_{permissions}
    {}
    const std::string& GetLoginName() const override { return name_; }
    const boost::uuids::uuid& GetUuid() const override { return id_; }
    int GetNumInstances() const override { return instance_count_; }
    void IncInstanceCounter() { ++instance_count_; }
    void DecInstanceCounter() { --instance_count_; }
    const std::string& GetPwd() const { return pwd_; }
    Permissions::ptr_t GetPermissions() const override {return permissions_;}
private:
    const std::string name_;
    const std::string pwd_;
    const boost::uuids::uuid id_;
    std::atomic_int instance_count_;
    const Permissions::ptr_t permissions_;
};

// Wrapper needed to maintain the instance count for the client
class ClientProxy : public Client
{
public:
    const std::string& GetLoginName() const override {
        return client_->GetLoginName();
    }

    const boost::uuids::uuid& GetUuid() const override {
        return client_->GetUuid();
    }

    int GetNumInstances() const override {
        return client_->GetNumInstances();
    }

    ClientProxy(const std::shared_ptr<ClientImpl>& c)
    : client_{c}
    {
        client_->IncInstanceCounter();
    }

    ~ClientProxy() {
        client_->DecInstanceCounter();
    }

    Permissions::ptr_t GetPermissions() const override {
        return client_->GetPermissions();
    }

private:
    const std::shared_ptr<ClientImpl> client_;
};

/*! Auth manager implementation
 *
 * This is a very simple implementation, using
 * wfded's configuration-file for authentication.
 *
 * Passwods are in plain text. This is sufficient for functional testing.
 *
 * I would not deploy a FTP server with passwords in plain text on any
 * production servers.
 */
class AuthManagerImpl : public AuthManager
{
public:
    AuthManagerImpl(Configuration& conf)
    {
        // Cache all the users
        for(auto node : conf.EnumNodes("")) {
            const auto node_path = node.name;
            auto links_to = conf.GetValue(node_path.c_str(), "");
            if (!links_to.empty()) {
                // Alias. Assert that the user already exist and add another
                // entry for it.
                auto cli_it = clients_.find(links_to);
                if (cli_it == clients_.end()) {
                    WAR_THROW_T(ExceptionNotFound, links_to);
                }
                clients_[node.name] = cli_it->second;
                LOG_TRACE1_FN << "Added alias " << log::Esc(node.name)
                              << " --> "
                              << log::Esc(cli_it->second->GetLoginName())
                              << " to the user-cache";
                continue;
            }

            auto perms = CreatePermissions(
                conf.GetConfigForPath((node_path + "/Paths").c_str()));

            const auto pwd = conf.GetValue((node_path + "/Passwd").c_str(), "");
            clients_[node.name] = std::make_shared<ClientImpl>(node.name, pwd,
                                                               perms);

            LOG_TRACE1_FN << "Added user " << node.name
                          << (pwd.empty() ? " (anonymous) " : " with password ")
                          << " to the user-cache";
        }
    }

    Client::ptr_t Login(const std::string& name,
                        const std::string& pwd) override
    {
        LOG_DEBUG_FN << "Authenticating user " << log::Esc(name)
            << (pwd.empty() ? " without " : " with ") << "password";

        // Search
        auto cli_it = clients_.find(name);
        if (cli_it == clients_.end())
            WAR_THROW_T(ExceptionNotFound, name);

        // Validate
        const auto& cli_pwd = cli_it->second->GetPwd();
        if (!cli_pwd.empty() && pwd.empty()) {
            WAR_THROW_T(ExceptionNeedPasswd, "Need password");
        }

        if (cli_pwd!= pwd) {
            WAR_THROW_T(ExceptionBadCredentials, "Password does not match");
        }

        // Create a client proxy
        LOG_DEBUG_FN << "Authenticated user "
            << log::Esc(cli_it->second->GetLoginName());
        return std::make_shared<ClientProxy>(cli_it->second);
    }

    void Join(Host::ptr_t) override {};

private:
    std::map<std::string, std::shared_ptr<ClientImpl>, ciLessLibC> clients_;
};


}}} // namespaces

