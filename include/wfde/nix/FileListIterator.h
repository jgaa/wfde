#pragma once

#include <sys/types.h>
#include <dirent.h>

namespace file_listing {

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
        : is_eof_{ false }, st_(st), name_{ name } {}

    /*! Constructor for a "fake" directoty iterator.
    *
    * This is used when we onlly want it to "iterate over" a single
    * entity (MLST). The path can point to a dir or a file.
    */
    FileListIterator(const Path& path, const boost::string_ref& name)
        : path_{ &path }, is_eof_{ false }, name_{ name } {}

    /*! Constructor for a real directory iterator */
    FileListIterator(const Path& path)
        : path_{ &path }, is_eof_{ false }
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
        vpaths_.push_back({ name.to_string(), st });
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

                static const struct stat empty_st {};
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

    file_len_t GetSize() const noexcept { return st_.st_size; }

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

        return{ unique_fact_buf, static_cast<std::size_t>(len) };
    }

private:
    void Advance() {
        if (IsFake()) {
            is_eof_ = true;
            return;
        }

        struct dirent *dirent = nullptr;
        if (!is_eof_) {
            errno = 0;
            dirent = readdir(dir_);
            if (UNLIKELY(dirent == nullptr)) {
                const auto errval = errno;
                if (errval) {
                    WAR_THROW_T(ExceptionIoError,
                                std::string("readdir failed with error ")
                                + std::to_string(errval));
                }
                is_eof_ = true;
            }
        }

        if (!is_eof_) {
            name_ = boost::string_ref(dirent->d_name, strlen(dirent->d_name));
        }
        else {
            if (vpaths_.empty()) {
                name_.clear(); // Done
                have_vpath_ = false;
            }
            else {
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
    struct stat st_ {};
    boost::string_ref name_;
    boost::filesystem::path stat_path_;
    mutable char unique_fact_buf[64]{};
    std::vector<std::pair<std::string, struct stat>> vpaths_;
};


} // namespace
