#pragma once

#include <string>
#include <sys/types.h>
#include <dirent.h>
#include <cstdio>
#include <time.h>

#include <boost/filesystem.hpp>

#include "wfde/wfde.h"
#include "war_error_handling.h"
#include "log/WarLog.h"

// TODO: Decouple
#include "ftp_protocol.h"

namespace war {
namespace wfde {

struct FtpState;

namespace file_listing {

using file_len_t = decltype(boost::filesystem::file_size(""));
using file_stat_t = decltype(boost::filesystem::status(""));

// We use our own naive directory iterator because boost:::filesystem
// maps to strings and reuqire extra calls to obtain the file-size.
class FileListIterator
{
public:
    FileListIterator() {}

    /*! Constructor for a placeholder iterator
     *
     * Tis is used when we want to pass a specific name/stat pair to the
     * formatter. It can not be iterated over.
     */
    FileListIterator(const boost::string_ref& name,
                     const struct stat& st)
    : is_eof_{false}, st_(st), name_ {name} {}

    /*! Constructor for a "fake" directoty iterator.
     *
     * This is used when we onlly want it to "iterate over" a single
     * entity (MLST). The path can point to a dir or a file.
     */
    FileListIterator(const Path& path, const boost::string_ref& name)
    : path_{&path}, is_eof_{false}, name_ {name} {}

    /*! Constructor for a real directory iterator */
    FileListIterator(const Path& path)
    : path_{&path}, is_eof_{false}
    {
        LOG_TRACE1_FN << "Opening directory for list: "
            << log::Esc(path.GetPhysPath().string());

        dir_ = opendir(path.GetPhysPath().string().c_str());
        if (!dir_) {
            log::Errno err;
            LOG_WARN_FN << "Failed to opendir() "
                << log::Esc(stat_path_.string()) << ' ' << err;
            WAR_THROW_T(ExceptionAccessDenied, path.GetPhysPath().string());
        }

        // Prepare the first entry
        Advance();
    }

    ~FileListIterator() {
        Close();
    }

    void Close() {
        if (dir_) {
            closedir(dir_);
            dir_ = nullptr;
        }
    }

    void AddVirtualPath(const boost::string_ref& name, const struct stat& st) {
        vpaths_.push_back({name.to_string(), st});
    }

    FileListIterator& operator ++() {
        Advance();
        return *this;
    }

    bool IsFake() const noexcept { return dir_ == nullptr; }

    // Intended only to detect eof
    bool operator != (const FileListIterator&) const {
        return !is_eof_ || have_vpath_;
    }

    /*! Get extended information (more than just the name) */
    void Stat() {
        stat_path_ = path_->GetPhysPath();

        if (!IsFake()) {
            // TODO: We may want to optimize here
            stat_path_ /= GetName().to_string();
        }
        if (!is_eof_) {
            if (UNLIKELY(stat(stat_path_.string().c_str(), &st_))) {
                log::Errno err;
                LOG_WARN_FN << "Failed to stat "
                    << log::Esc(stat_path_.string()) << ' ' << err;

                static const struct stat empty_st{};
                st_ = empty_st;
            }
        }
    }

    FileListIterator& operator * ()
    {
        return *this;
    }

    const FileListIterator& operator * () const
    {
        return *this;
    }

    FileListIterator * operator -> ()
    {
        return this;
    }

    const FileListIterator * operator -> () const
    {
        return this;
    }

    file_len_t GetSize() const noexcept  { return st_.st_size; }

    const timespec& GetModifyTime() const noexcept { return st_.st_mtim; }

    bool IsDirectory() const noexcept { return S_ISDIR(st_.st_mode); }

    const boost::string_ref& GetName() const noexcept {
        return name_;
    }

    const Path *GetPath() const noexcept { return path_; }

    /* TODO: This is unsafe - we should not expose device and
     * inode ID's to clients.
     *
     * We should in stead return a unique hash, that may contain the
     * inode/dev id's and a random salt that changes from server-session
     * to server-session (so that people who know the algorithm, still can't
     * deduce the inode and dev numbers. However, we need a fast hashing
     * algorithm to prevent DoS by clients repeatedly listing large
     * directories.
     */
    boost::string_ref GetUniqueFact() const noexcept {
        if (path_ == nullptr) {
            return name_; // Pure virtual
        }

        const auto len = snprintf(unique_fact_buf, sizeof(unique_fact_buf),
                                  "%x:%llx",
                 static_cast<int>(st_.st_dev),
                 static_cast<long long>(st_.st_ino));

        return {unique_fact_buf, static_cast<std::size_t>(len)};
    }

private:
    void Advance() {
        if (IsFake()) {
            is_eof_ = true;
            return;
        }

        if (!is_eof_) {
            if (UNLIKELY(readdir_r(dir_, &dirent_, &result_) != 0)) {
                    WAR_THROW_T(ExceptionIoError, "readdir64_r failed");
            }

            if (UNLIKELY(!result_)) {
                is_eof_ = true;
            }
        }

        if (!is_eof_) {
            name_ = boost::string_ref(result_->d_name, strlen(result_->d_name));
        } else {
            if (vpaths_.empty()) {
                name_.clear(); // Done
                have_vpath_ = false;
            } else {
                name_ = vpaths_.back().first;
                st_ = vpaths_.back().second;
                vpaths_.pop_back();
                have_vpath_ = true;
            }
        }
    }

    const Path *path_ = nullptr;
    decltype(opendir("")) dir_ = nullptr;
    bool is_eof_ = true;
    bool have_vpath_ = false;
    struct dirent dirent_{};
    struct dirent *result_{};
    struct stat st_{};
    boost::string_ref name_;
    boost::filesystem::path stat_path_;
    mutable char unique_fact_buf[64]{};
    std::vector<std::pair<std::string, struct stat>> vpaths_;
};

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

                        st.st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH
                            | S_IXUSR | S_IXGRP | S_IXOTH;
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

