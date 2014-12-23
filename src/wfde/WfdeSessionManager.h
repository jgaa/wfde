#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <deque>
#include <unordered_map>

#include <boost/functional/hash.hpp>

#include "wfde/wfde.h"
#include "tasks/WarPipeline.h"
#include "log/WarLog.h"
#include "war_uuid.h"

namespace war {
namespace wfde {
namespace impl {

class WfdeSessionManager : public SessionManager
{
public:
    WfdeSessionManager(Threadpool& tp);
    ~WfdeSessionManager();

    Session::ptr_t CreateSession(const SessionParams& sc) override;

    Session::ptr_t GetSession(const boost::uuids::uuid& id) override;

    sessions_list_t GetSessions() override;

    void CloseSession(const boost::uuids::uuid& id) override;

private:
    using sessions_t = std::unordered_map<boost::uuids::uuid,
        Session::wptr_t, boost::hash<boost::uuids::uuid>>;

    /*! Part of the session manager for each thread owning session(s)
     */
    class ThreadStub
    {
    public:
        ThreadStub(SessionManager& parent, Pipeline& pipeline)
        : parent_{parent}, pipeline_{pipeline}
        {}

        void AttachSession(Session::ptr_t session) {
            thd_sessions_.push_back(session);
        }

        void DetachSession(const Session::ptr_t& session) {
            auto sesit = std::find(thd_sessions_.begin(), thd_sessions_.end(),
                                   session);

            if (sesit == thd_sessions_.end()) {
                LOG_WARN_FN << *session << " not found!";
            } else {
                thd_sessions_.erase(sesit);
            }
        }

        bool IsEmpty() const { return thd_sessions_.empty(); }

        /*! Calls OnHousekeeping on all thread-local sessions */
        void OnThdTimer();

        Pipeline& GetPipeline() { return pipeline_; }

    private:
        std::deque<Session::ptr_t> thd_sessions_;
        SessionManager& parent_;
        Pipeline& pipeline_;
    };

    void OnThdTimer(int id);
    void ScheduleThdTimer(int id);

    mutable std::mutex mutex_;
    sessions_t sessions_;

    /*! Thread-local pointers.
     *  Each thread will access it's slot without syncronization.
     */
    std::vector<std::unique_ptr<ThreadStub>> stubs_;
};


}}} // namespaces
