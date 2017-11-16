#pragma once

#include <string>
#include <cstdio>
#include <time.h>
#include <sys/stat.h>

#include <boost/filesystem.hpp>

#include <wfde/wfde.h>
#include <warlib/error_handling.h>
#include <warlib/WarLog.h>

// TODO: Decouple
#include "ftp_protocol.h"

namespace war {
namespace wfde {

struct FtpState;

namespace file_listing {
using file_len_t = decltype(boost::filesystem::file_size(""));
using file_stat_t = decltype(boost::filesystem::status(""));
} //namespace

#ifdef WIN32
#	include <wfde/win/FileListIterator.h>
#else
#	include <wfde/nix/FileListIterator.h>
#endif

namespace file_listing {

template <typename formatT>
class Formatter {
public:
    Formatter(char *buffer, const char *end, const FtpState& state)
    : buffer_{buffer}, cur_{buffer}, end_{end}, format_{state}
    {}

    // Print the entry or return -1 if there is not enough buffer-space.
    // If we return -1, we reset the current pointer, so that the next
    // Print will happen on the beginning of the buffer.
    int Print(const FileListIterator& fli)
    {

        // Calculate approximately bytes needed
        const auto approx_bytes_needed = format_.CalculateSize(fli);
        if ((cur_ + approx_bytes_needed) >= end_) {
            cur_ = buffer_;
            return -1;
        }

        // Print
        const auto bytes_consumed = format_.PrintIt(cur_, fli);
        WAR_ASSERT(bytes_consumed <= approx_bytes_needed);
        cur_ += bytes_consumed;

        LOG_TRACE4_FN << "Consumed " << bytes_consumed << " bytes";

        return bytes_consumed;
    }

    formatT& GetFormat() { return format_; }

private:
    char *const buffer_;
    char *cur_;
    const char *end_;
    formatT format_;
};

template <typename FormatT>
class DirLister
{
public:
    DirLister(const Path& path, Session& session, const FtpState& state)
    : current_path_{&path}, session_{session}
    , fli_(path)
    , formatter_(buffer_.data(), buffer_.data() + buffer_.size(), state)
    , show_hidden_{state.list_hidden_files && path.CanSeeHiddenFiles()}
    {
        decltype(session_.GetVpaths("")) vpaths;

        // Get vpaths for the current path
        auto all_vpaths = session_.GetVpaths(current_path_->GetVirtualPath());

        // Add the vpaths that does not exist as physical directories
        for(const auto& name : all_vpaths) {
            auto fs_path = path.GetPhysPath();
            fs_path /= name.to_string();

            if (!boost::filesystem::exists(fs_path)) {
                LOG_TRACE4_FN << "Adding pure virtual path "
                    << log::Esc(name);

                // Get the real path and stat it
                const std::string sub_vpath = path.GetVirtualPath()
                    + '/' + name.to_string();

                try {
                    auto vpath = session.GetPath(sub_vpath,
                                                 Path::Type::DIRECTORY);

                    struct stat st{};
                    if (stat(vpath->GetPhysPath().string().c_str(), &st)) {
                        LOG_WARN_FN << "Unable to stat() directory "
                            << log::Esc(vpath->GetPhysPath().string())
                            << " referenced by vpath "
                            << log::Esc(vpath->GetVirtualPath())
                            << " for " << session;

#ifdef WIN32
                        st.st_mode = _S_IFDIR;
#else
                        st.st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH
                            | S_IXUSR | S_IXGRP | S_IXOTH;
#endif
                    }

                    fli_.AddVirtualPath(name, st);

                } WAR_CATCH_ERROR;
            }
        }
    }

    DirLister(const Path& path, Session& session, const FtpState& state,
              bool /*Just to get a different signature */)
    : current_path_{&path}, session_{session}
    , fli_{path, path.GetVirtualPath()}
    , formatter_(buffer_.data(), buffer_.data() + buffer_.size(), state)
    {
    }

    // Returns true if there is more to list
    bool List() {
        bytes_used_ = 0;
        WAR_ASSERT(current_path_);

        for(; fli_ != end_; ++fli_) {

            const auto& name = fli_.GetName();
            const auto name_len = name.size();

            if (name[0] == '.') {
                if (!(show_hidden_
                    || (name_len == 1)
                    || ((name_len == 2) && (name[1] == '.'))))

                    continue; // hide UNIX hidden file
            }

            // TODO: Only Stat if we actually need the extra info
            fli_->Stat();
            const auto bytes = formatter_.Print(*fli_);
            if (UNLIKELY(bytes == -1)) {
                // End of buffer
                LOG_TRACE4_FN << "Returning batch of "
                    << bytes_used_ << " bytes";
                return true;
            }

            bytes_used_ += bytes;
        }

        LOG_TRACE2_FN << "End of listing. This batch was "
            << bytes_used_ << " bytes";

        return false;
    }

    File::const_buffer_t GetBuffer() const noexcept
    {
        WAR_ASSERT(bytes_used_ <= buffer_.size());
        return {buffer_.data(), bytes_used_};
    }

    size_t GetBytesUsed() const noexcept {
        return bytes_used_;
    }

    void Close() {
        fli_.Close();
    }

    Formatter<FormatT>& GetFormatter() { return formatter_; }

private:
    const Path *current_path_ = nullptr;
    Session& session_;
    size_t bytes_used_ = 0; // Bytes used in the current batch
    FileListIterator fli_;
    const FileListIterator end_;
    std::array<char, 1024 * 16> buffer_;
    Formatter<FormatT> formatter_;
    const bool show_hidden_ = false;
};


}}} // namespaces

