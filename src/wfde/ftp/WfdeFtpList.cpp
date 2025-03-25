
#include "war_wfde.h"
#include "WfdeFtpList.h"

namespace war {
namespace wfde {
namespace impl {

void print_timeval(char *&cur, const timespec& ts)
{
    struct tm tm{};
    if (!war_gmtime(ts.tv_sec, tm)) {
        tm.tm_mday = 1;
    }

    const auto len = strftime(cur, 16, "%Y%m%d%H%M%S", &tm);

    WAR_ASSERT(len == 14);
    cur += len;
}


std::string GetMlstFacts(const Path& path, Session& session,
                                const FtpState& state) {
    DirListerMlst mlst(path, session, state);

    mlst.List();
    const auto b = mlst.GetBuffer();

    std::string rval {static_cast<const char *>(b.data()),
        boost::asio::buffer_size(b)};

    if (rval.size() > 2) {
        // Remove trailing crlf -- we don';t want it for mlst
        rval.resize(rval.size() -2);
    }

    return rval;
}

std::string GetRfc3659FileTime(const Path& path) {
    if (!path.CanList()) {
        WAR_THROW_T(ExceptionAccessDenied, "Missing CAN_LIST");
    }

    char buf[16]{};
   
#ifdef WIN32
    const auto handle = CreateFile(path.GetPhysPath().string().c_str(), 
        GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (handle == INVALID_HANDLE_VALUE) {
        log::Errno err;
        LOG_WARN_FN << "Failed to stat [CreateFile]: "
            << log::Esc(path.GetPhysPath().string())
            << err;
        WAR_THROW_T(ExceptionIoError, "stat() failed");
    }

    FILETIME ft_create{}, ft_access{}, ft_write{};
    const bool result = GetFileTime(handle, &ft_create, &ft_access, &ft_write);
    log::Errno system_err;
    CloseHandle(handle);

    if (!result) {
        LOG_WARN_FN << "Failed to stat [GetFileTime]: "
            << log::Esc(path.GetPhysPath().string())
            << system_err;
        WAR_THROW_T(ExceptionIoError, "stat() failed");
    }

    timespec ts{};
    detail::filetime_to_timespec(&ft_write, &ts);
    char *p{ buf };
    print_timeval(p, ts);
#else
    struct stat st {};
    if (stat(path.GetPhysPath().string().c_str(), &st)) {
        log::Errno err;
        LOG_WARN_FN << "Failed to stat "
            << log::Esc(path.GetPhysPath().string())
            << err;
        WAR_THROW_T(ExceptionIoError, "stat() failed");
    }

    char *p{ buf };
    print_timeval(p, st.st_mtim);
#endif
    return {buf};
}

}}}
