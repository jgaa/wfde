#include "war_wfde.h"

#include "tasks/WarThreadpool.h"
#include "log/WarLog.h"
#include "war_uuid.h"

#include "WfdeClient.h"
#include "WfdeSession.h"
#include "WfdeFile.h"

#define LOCK lock_guard<mutex> lock__(mutex_)

using namespace std;

std::ostream& operator << (std::ostream& o, const war::wfde::Session& ses)
{
    return o << "{Session " << boost::uuids::to_string(ses.GetUuid()) << '}';
}

namespace war {
namespace wfde {
namespace impl {

WfdeSession::WfdeSession(const SessionManager::SessionParams& sp)
: uuid_(boost::uuids::random_generator()())
, client_{sp.client}
, protocol_{sp.protocol}
, socket_{sp.socket}
, session_time_out_secs_{sp.session_time_out_secs}
, tp_thread_id_{sp.socket->GetPipeline().GetId()}
{
    WAR_ASSERT(protocol_ && "Must have a protocol");
    WAR_ASSERT(socket_ && "Must have a socket");
    WAR_ASSERT(tp_thread_id_ >= 0 && "Must have a thread slot ID");
    Touch();
}

WfdeSession::~WfdeSession()
{
}

bool WfdeSession::AuthenticateWithPasswd(const string& name, const string& pwd)
{
    {
        std::lock_guard<std::mutex> lock__(mutex_);
        if (client_) {
            LOG_ERROR_FN << "User already logged on " << log::Esc(name)
                << ' ' << *this;
            WAR_THROW_T(ExceptionAlreadyLoggedIn, "");
        }
    }

    try {
        auto client = GetHost().GetAuthManager().Login(name, pwd);

        // TODO: Update permissions
        auto perms = client->GetPermissions();
        if (perms) {
            LOG_DEBUG_FN << "Merging the users perms into the session "
                << *this;
            auto ep = perms->Copy();
            ep->Merge(*GetPermissions());
            SetPermissions(ep);

            LOG_TRACE1_FN << "Effective perms for session " << *this
                << ": " << *ep;
        }

        LOG_NOTICE_FN << *client << " logged on to " << *this;
        SetClient(move(client));
        return true;
    } catch(AuthManager::ExceptionBadCredentials) {
        // TODO: Notify security manager
        LOG_NOTICE_FN << "Bad credentials while attempting login as "
            << log::Esc(name) << " to " << *this;
    } catch(AuthManager::ExceptionNeedPasswd) {
        LOG_TRACE1_FN << "Need password for user " << log::Esc(name)
            << " on " << *this;
    } catch(ExceptionNotFound&) {
        // TODO: Notify security manager
        LOG_TRACE1_FN << "Unknown user user " << log::Esc(name)
            << " on " << *this;
    }

    return false;
}


void WfdeSession::Close()
{
    if (!closed_) {
        WAR_ASSERT(owning_thread_id_ == std::this_thread::get_id());
        LOG_TRACE1_FN << "Closing " << *this;
        socket_->GetSocket().close();
        closed_ = true;
    }
}

unique_ptr< File > WfdeSession::OpenFile(const vpath_t& path,
                                         const File::FileOperation operation)
{
    auto file_path = GetPath(path, Path::Type::FILE);

    // Authorization
    switch(operation) {
        case File::FileOperation::APPEND:
            if (!file_path->CanWrite()) {
                LOG_DEBUG_FN << "Missing CAN_WRITE for "  << operation;
                WAR_THROW_T(ExceptionAccessDenied, path);
            }
            break;
        case File::FileOperation::READ:
            if (!file_path->CanRead()) {
                LOG_DEBUG_FN << "Missing CAN_READ for " << operation;
                WAR_THROW_T(ExceptionAccessDenied, path);
            }
            break;
        case File::FileOperation::WRITE_NEW:
            if (!file_path->CanCreateFile()) {
                LOG_DEBUG_FN << "Missing CAN_CREATE_FILE for " << operation;
                WAR_THROW_T(ExceptionAccessDenied, path);
            }
            // Fall trough
        case File::FileOperation::WRITE:
            if (!file_path->CanWrite()) {
                LOG_DEBUG_FN << "Missing CAN_WRITE for " << operation;
                WAR_THROW_T(ExceptionAccessDenied, path);
            }
            break;
        default:
            WAR_ASSERT(false && "Unsupported operation");
    }

    auto file = make_unique<WfdeFile>(file_path->GetPhysPath(), operation);

    LOG_DEBUG_FN << "Opened " << *file
        << ' ' << log::Esc(file_path->GetVirtualPath())
        << " as physical file " << log::Esc(file_path->GetPhysPath().string())
        << " for " << operation
        << " in " << *this;

    return move(file);
}

void WfdeSession::DeleteFile(const Session::vpath_t& path)
{
    auto file_path = GetPath(path, Path::Type::FILE);

    if (!file_path->CanDeleteFile()) {
         LOG_DEBUG_FN << "Missing CAN_DELETE_FILE";
         WAR_THROW_T(ExceptionAccessDenied, path);
    }

    if (!file_path->Exists()) {
        WAR_THROW_T(ExceptionNotFound, file_path->GetPhysPath().string());
    }

    LOG_NOTICE_FN << "Deleting file " << log::Esc(file_path->GetPhysPath().string());

    boost::system::error_code ec;
    boost::filesystem::remove(file_path->GetPhysPath(), ec);

    if (ec) {
        LOG_WARN_FN << "Failed to delete the file "
            << log::Esc(file_path->GetPhysPath().string())
            << ' '
            << ec;
        WAR_THROW_T(ExceptionAccessDenied, file_path->GetPhysPath().string());
    }
}

void WfdeSession::CreateDirectory(const Session::vpath_t& path)
{
    auto dir_path = GetPath(path, Path::Type::DIRECTORY);

    if (!dir_path->CanCreateDir()) {
         LOG_DEBUG_FN << "Missing CAN_CREATE_DIR";
         WAR_THROW_T(ExceptionAccessDenied, path);
    }

    if (dir_path->Exists()) {
        WAR_THROW_T(ExceptionAlreadyExist, dir_path->GetPhysPath().string());
    }

    LOG_NOTICE_FN << "Creating directory " << log::Esc(dir_path->GetPhysPath().string());

    boost::system::error_code ec;
    boost::filesystem::create_directory(dir_path->GetPhysPath(), ec);

    if (ec) {
        LOG_WARN_FN << "Failed to create the directory "
            << log::Esc(dir_path->GetPhysPath().string())
            << ' '
            << ec;
        WAR_THROW_T(ExceptionAccessDenied, dir_path->GetPhysPath().string());
    }
}

void WfdeSession::DeleteDirectory(const Session::vpath_t& path)
{
    auto dir_path = GetPath(path, Path::Type::DIRECTORY);

    if (!dir_path->CanDeleteDir()) {
         LOG_DEBUG_FN << "Missing CAN_DELETE_DIR";
         WAR_THROW_T(ExceptionAccessDenied, path);
    }

    if (!dir_path->Exists()) {
        WAR_THROW_T(ExceptionNotFound, dir_path->GetPhysPath().string());
    }

    LOG_NOTICE_FN << "Deleting directory " << log::Esc(dir_path->GetPhysPath().string());

    // TODO: Add option to allow recursive rmdir?
    // It's in boost, but it's a bit dangerous to enable by default.
    boost::system::error_code ec;
    boost::filesystem::remove(dir_path->GetPhysPath(), ec);

    if (ec) {
        LOG_WARN_FN << "Failed to delete the directory "
            << log::Esc(dir_path->GetPhysPath().string())
            << ' '
            << ec;
        WAR_THROW_T(ExceptionAccessDenied, dir_path->GetPhysPath().string());
    }
}

void WfdeSession::Rename(const Session::vpath_t& from, const Session::vpath_t& to)
{
    auto from_path = GetPath(from, Path::Type::ANY);
    auto to_path = GetPath(to, Path::Type::ANY);

    if (!from_path->CanRename()) {
        WAR_THROW_T(ExceptionAccessDenied, "CAN_RENAME");
    }

    if (!from_path->Exists()) {
        WAR_THROW_T(ExceptionNotFound, from_path->GetPhysPath().string());
    }

    if (to_path->Exists()) {
        LOG_DEBUG_FN << "The path " << to_path->GetPhysPath().string()
            << " already exists.";
        WAR_THROW_T(ExceptionAlreadyExist, to_path->GetPhysPath().string());
    }

    if (!from_path->IsSameParentDir(*to_path)) {
        const bool is_dir = boost::filesystem::is_directory(
            from_path->GetPhysPath());
        if (is_dir && !to_path->CanCreateDir()) {
             WAR_THROW_T(ExceptionAccessDenied, "CAN_CREATE_DIR");
        }


        if (!is_dir && !to_path->CanCreateFile()) {
             WAR_THROW_T(ExceptionAccessDenied, "CAN_CREATE_FILE");
        }
    }

    LOG_NOTICE_FN << "Renaming "
            << log::Esc(from_path->GetPhysPath().string())
            << " to " << log::Esc(to_path->GetPhysPath().string());

    boost::system::error_code ec;
    boost::filesystem::rename(from_path->GetPhysPath(), to_path->GetPhysPath(), ec);

    if (ec) {
        LOG_WARN_FN << "Failed to rename "
            << log::Esc(from_path->GetPhysPath().string())
            << " --> " << log::Esc(to_path->GetPhysPath().string())
            << ' ' << ec;
        WAR_THROW_T(ExceptionAccessDenied, "boost::filesystem::rename failed");
    }
}


File::fpos_t WfdeSession::GetFileLen(const vpath_t& path)
{
    auto file_path = GetPath(path, Path::Type::FILE);

    if (!file_path->CanList() && !file_path->CanRead()) {
        WAR_THROW_T(ExceptionAccessDenied, path);
    }

    boost::system::error_code ec;
    auto rval = boost::filesystem::file_size(file_path->GetPhysPath(), ec);
    if (ec) {
        WAR_THROW_T(ExceptionNotFound, file_path->GetPhysPath().string());
    }
    return rval;
}

void WfdeSession::SetCwd(const vpath_t& path)
{
    // Authorization
    auto dir = GetPath(path, Path::Type::DIRECTORY);
    if (!dir->CanEnter()) {
        LOG_DEBUG_FN << "The client does not have CAN_ENTER access to "
            << log::Esc(dir->GetVirtualPath());
        WAR_THROW_T(ExceptionAccessDenied, "CAN_ENTER");
    }

    if (!dir->Exists()) {
         LOG_DEBUG_FN << "The directory "
            << log::Esc(dir->GetPhysPath().string())
            << " does not exist.";
         WAR_THROW_T(ExceptionAccessDenied, "No such Dir");
    }

    LOG_DEBUG_FN << "Changing CWD to " << log::Esc(dir->GetVirtualPath());
    cwd_ = dir->GetVirtualPath();
}

WfdeSession::vpath_t WfdeSession::GetCwd() const
{
    return cwd_;
}

unique_ptr< Path > WfdeSession::GetPath(const string& vpath, Path::Type type)
{
    LOG_TRACE2_FN << "Dealing with path " << log::Esc(vpath);

    Path::vpath_t remaining;

    // Get the best match from Permissions
    const auto& best_match = permissions_->GetPath(Path::Normalize(vpath, cwd_),
                                                   &remaining);

    // Return a copy of the desired type, covering the full vpath

    LOG_TRACE4_FN << "Remaining = " << log::Esc(remaining);

    WAR_ASSERT(remaining.empty() || (remaining.front() != '/'));
    return best_match.CreateSubpath(remaining, type);
}

vector< boost::string_ref > WfdeSession::GetVpaths(const Session::vpath_t& path)
{
    vector< boost::string_ref > rval;
    std::set<boost::string_ref> haves;

    const auto all_paths = permissions_->GetPaths();

    for(const auto p : all_paths) {
        const auto& vpath = p->GetVirtualPath();

        if (vpath.size() <= path.size())
            continue;

        if (memcmp(vpath.c_str(), path.c_str(), path.size()))
            continue;

        if (vpath[path.size() -1] != '/')
            continue;

        auto pos = vpath.find_first_of('/', path.size() +1);
        if (pos == vpath.npos)
            pos = vpath.size();

        const boost::string_ref name(&vpath[path.size()], pos - (path.size()));

        if (haves.find(name) != haves.end())
            continue;

        LOG_TRACE4_FN << "Adding vpath " << log::Esc(name);
        haves.insert(name);
        rval.push_back(name);
    }

    return rval;
}


void WfdeSession::SetPermissions(const Permissions::ptr_t& perms)
{
    permissions_ = perms;
}

unique_ptr< Path > WfdeSession::GetPathForListing(const string& vpath)
{
    auto path = GetPath(vpath, Path::Type::DIRECTORY);
    if (!path->Exists()) {
        LOG_DEBUG_FN << "Failed to get the vpath " << log::Esc(vpath);
        WAR_THROW_T(ExceptionNotFound, vpath);
    }

    if (!path->CanList()) {
        LOG_DEBUG_FN << "Missing CAN_LIST for vpath " << log::Esc(vpath);
        WAR_THROW_T(ExceptionAccessDenied, vpath);
    }

    return path;
}

bool WfdeSession::OnHousekeeping() {
    WAR_ASSERT(owning_thread_id_ == std::this_thread::get_id());
    UpdateIdleTime();

    if (cached_idle_time_ > session_time_out_secs_) {
        LOG_DEBUG_FN << *this << " Timed out. cached_idle_time_="
            << cached_idle_time_
            << ", session_time_out_secs_=" << session_time_out_secs_;
        return false;
    }

    return true;
}

} // impl

} // wfde
} // war
