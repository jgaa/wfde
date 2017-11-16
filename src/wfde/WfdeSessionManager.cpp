#include "war_wfde.h"

#include <string>

#include <warlib/WarThreadpool.h>
#include <warlib/WarLog.h>
#include <warlib/uuid.h>
#include <warlib/helper.h>

#include "WfdeClient.h"
#include "WfdeSessionManager.h"
#include "WfdeSession.h"

#define LOCK lock_guard<mutex> lock__(mutex_)

using namespace std;
using namespace std::string_literals;

namespace war {
namespace wfde {
namespace impl {

WfdeSessionManager::WfdeSessionManager(Threadpool& tp)
: stubs_(tp.GetNumThreads())
{
}

WfdeSessionManager::~WfdeSessionManager()
{
}


Session::ptr_t WfdeSessionManager::CreateSession(const SessionParams& sc)
{
    auto& stub = stubs_[sc.socket->GetPipeline().GetId()];

    // Make sure that we have the thread-local stub
    if (!stub) {
        stub = make_unique<ThreadStub>(*this, sc.socket->GetPipeline());

        LOG_DEBUG_FN << "Starting Session-Timer for thread-specific stub";
        ScheduleThdTimer(sc.socket->GetPipeline().GetId());
    }

    auto session = make_shared<WfdeSession>(sc);
    stub->AttachSession(session);

    {
        LOCK;
        sessions_[session->GetUuid()] = session;
    }

    LOG_DEBUG_FN << "Created session " << *session << ' '
        << " on " << session->GetSocket();

    return session;
}

Session::ptr_t WfdeSessionManager::GetSession(const boost::uuids::uuid& id)
{
    {
        LOCK;
        auto session = sessions_.find(id);
        if (session != sessions_.end()) {
            if (auto ref = session->second.lock())
                return ref;
        }
    }

    WAR_THROW_T(ExceptionNotFound, "Session "s + boost::uuids::to_string(id));
}

SessionManager::sessions_list_t WfdeSessionManager::GetSessions()
{
    decltype(GetSessions()) rval;

    {
        LOCK;
        rval.reserve(sessions_.size());
        for(auto& s: sessions_) {
            rval.emplace_back(s.second);
        }
    }
    return rval;
}

void WfdeSessionManager::CloseSession(const boost::uuids::uuid& id)
{
    auto ref = GetSession(id);
    const auto thd_id = ref->GetThreadId();

    WAR_ASSERT(thd_id >= 0);
    WAR_ASSERT(thd_id < static_cast<decltype(thd_id)>(stubs_.size()));

    auto& stub = stubs_[thd_id];
    WAR_ASSERT(stub);

    LOG_TRACE2_FN << "Closing " << *ref << " with thd-stub in slot " << thd_id;

    try {
        ref->Close();
    } WAR_CATCH_ERROR;

    {
        LOCK;
        sessions_.erase(id);
    }

    if (stub) {
        stub->DetachSession(ref);
    } else {
        LOG_ERROR_FN << "Missing thd-slot for session " << id;
    }
}


void WfdeSessionManager::ScheduleThdTimer(const int id)
{
    WAR_ASSERT(id >= 0);
    WAR_ASSERT(id < static_cast<decltype(id)>(stubs_.size()));
    auto& stub = stubs_[id];

    WAR_ASSERT(stub);
    stub->GetPipeline().PostWithTimer(
        {bind(&WfdeSessionManager::OnThdTimer,this, id),
        "Session Thread Timer"}, 3000);
}

void WfdeSessionManager::OnThdTimer(const int id) {

    WAR_ASSERT(id >= 0);
    WAR_ASSERT(id < static_cast<decltype(id)>(stubs_.size()));
    auto& stub = stubs_[id];

    if (!stub)
        return;

    if (stub->IsEmpty()) {
        LOG_DEBUG_FN << "Ending Session-Timer for thread-specific stub - "
            "no sessions on this thread";

        stub.reset();
        return;
    }

    try {
        stub->OnThdTimer();
    } WAR_CATCH_ERROR;

    ScheduleThdTimer(id);
}

void WfdeSessionManager::ThreadStub::OnThdTimer()
{
    for(auto& session : thd_sessions_) {
        try {
            if (!session->OnHousekeeping()) {
                const auto id = session->GetUuid();
                session->GetSocket().GetPipeline().Post({[this, id]() {
                    try {
                        parent_.CloseSession(id);
                    } catch (const ExceptionNotFound&) {
                        LOG_TRACE3 << "Ignoring expired session "
                            << boost::uuids::to_string(id);
                    } WAR_CATCH_ERROR;
                }, "Housekeeping: Close Session"});
            }
        } WAR_CATCH_ERROR;
    }
}


} // impl

SessionManager::ptr_t SessionManager::Create(Threadpool& tp)
{
    return make_shared<impl::WfdeSessionManager>(tp);
}


} // wfde
} // war
