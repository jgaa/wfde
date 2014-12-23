#pragma once

#include <atomic>
#include <thread>
#include <chrono>

#include <boost/uuid/uuid.hpp>

#include "wfde/wfde.h"
#include "tasks/WarPipeline.h"

namespace war {
namespace wfde {
namespace impl {

class WfdeSession : public Session
{
public:
    WfdeSession(const SessionManager::SessionParams& sp);
    ~WfdeSession();

    bool AuthenticateWithPasswd(const std::string& name,
                                const std::string& pwd) override;

    void SetPermissions(const Permissions::ptr_t& perms) override;

    Permissions::ptr_t GetPermissions() override {
        return permissions_;
    }

    vpath_t GetCwd() const override;

    void SetCwd(const vpath_t& path) override;

    std::unique_ptr<Path> GetPath(const std::string& vpath,
                                  Path::Type type) override;

    std::unique_ptr<Path> GetPathForListing(const std::string& vpath) override;

    std::unique_ptr<File> OpenFile(const vpath_t& path,
                                   File::FileOperation operation) override;

    std::vector<boost::string_ref> GetVpaths(const vpath_t& path) override;

    void DeleteFile(const vpath_t& path) override;

    void CreateDirectory(const vpath_t& path) override;

    void DeleteDirectory(const vpath_t& path) override;

    void Rename(const vpath_t& from, const vpath_t& to) override;

    File::fpos_t GetFileLen(const vpath_t& path) override;

    const boost::uuids::uuid& GetUuid() const override { return uuid_; };

    Client::ptr_t GetClient() override {
        std::lock_guard<std::mutex> lock__(mutex_);
        return client_;
    };

    void SetClient(std::shared_ptr<Client> client) override {
         std::lock_guard<std::mutex> lock__(mutex_);
         client_ = std::move(client);
    }

    Protocol& GetProtocol() override {
        return *protocol_;
    }

    Socket& GetSocket() override {
        return *socket_;
    }

    Host& GetHost() override {
        return protocol_->GetHost();
    }

    SessionData& GetSessionData() override {
        WAR_ASSERT(session_data_);
        return *session_data_;
    }

    boost::asio::ip::tcp::endpoint GetLocalEndpoint() override {
        return socket_->GetSocket().local_endpoint();
    }

    boost::asio::ip::tcp::endpoint GetRemoteEndpoint() override {
        return socket_->GetSocket().remote_endpoint();
    }

    std::chrono::system_clock::time_point GetLoginTime() const override {
        return login_time_;
    }

    std::chrono::steady_clock::duration GetIdleTime() const override {
        return  {std::chrono::steady_clock::now() - stable_login_time_};
    }

    std::chrono::steady_clock::duration GetElapsedTime() const override {
        return  {std::chrono::steady_clock::now() - stable_login_time_};
    }

    void Add(const std::shared_ptr<Data>& data) {
        owned_data_.emplace_back(data);
    }

    void Set(SessionData *data) {
        session_data_ = data;
    }

    void Touch() override {
        WAR_ASSERT(owning_thread_id_ == std::this_thread::get_id());
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::steady_clock::duration cached_duration{now - last_activity_cached_time_};
        last_activity_time_ = now;

        if (std::chrono::duration_cast<std::chrono::seconds>(cached_duration).count()) {
            // We have crossed a seconds bondary - update the cached
            // idle time values
            last_activity_cached_time_ = now; // non-thread-safe
            cached_idle_time_ = 0; //thread-safe
        }
    }

    bool OnHousekeeping() override;

    void Close() override;

    int GetThreadId() const noexcept override {
        return tp_thread_id_;
    }

protected:
    std::chrono::steady_clock::duration GetIdleTime_() const
    {
        return  {std::chrono::steady_clock::now() - stable_login_time_};
    }

    void UpdateIdleTime() {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::steady_clock::duration duration{now - last_activity_time_};
        cached_idle_time_ = std::chrono::duration_cast<std::chrono::seconds>(
            duration).count();
    }

private:
    boost::uuids::uuid uuid_;
    Client::ptr_t client_;
    const Protocol::ptr_t protocol_;
    const Socket::ptr_t socket_;
    const std::chrono::system_clock::time_point login_time_;
    const std::chrono::steady_clock::time_point stable_login_time_;
    std::chrono::steady_clock::time_point last_activity_time_;
    std::chrono::steady_clock::time_point last_activity_cached_time_;
    mutable std::mutex mutex_;
    std::atomic_int_fast32_t cached_idle_time_; // seconds
#ifdef DEBUG
    const std::thread::id owning_thread_id_ = std::this_thread::get_id();
#endif
    bool closed_ { false };
    std::vector<std::shared_ptr<Data>> owned_data_;
    SessionData *session_data_ = nullptr;
    vpath_t cwd_ = "/";
    Permissions::ptr_t permissions_;
    const uint32_t session_time_out_secs_;
    const int tp_thread_id_;
};

} // namespace impl
} // namespace wfde
} // namespace war
