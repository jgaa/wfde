#pragma once


namespace file_listing {

namespace detail
{
class FindFileWrapper
{
public:
    FindFileWrapper() = default;
    FindFileWrapper(const boost::filesystem::path& path) 
    {
        fh_ = FindFirstFile(path.string().c_str(), &ffd_);
        if (fh_ == INVALID_HANDLE_VALUE) {
            log::Errno err;
            LOG_WARN_FN << "Failed to FindFirstFile() "
                << log::Esc(path.string()) << ' ' << err;
            WAR_THROW_T(ExceptionAccessDenied, path.string());
        }
        eof_ = false;
    }

    ~FindFileWrapper() {
        Close();
    }

    bool IsEof() const noexcept {
        return eof_;
    }

    const WIN32_FIND_DATA& GetData() const noexcept {
        return ffd_;
    }

    bool Advance() {
        WAR_ASSERT(!IsEof());
        eof_ = (FindNextFile(fh_, &ffd_) == FALSE);
        return !eof_;
    }

    void Close() noexcept {
        if (fh_ != INVALID_HANDLE_VALUE) {
            FindClose(fh_);
            fh_ = INVALID_HANDLE_VALUE;
        }
    }

    bool IsInitialized() const noexcept {
        return fh_ != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE fh_{ INVALID_HANDLE_VALUE };
    bool eof_ = true;
    WIN32_FIND_DATA ffd_{};
};


/* time between jan 1, 1601 and jan 1, 1970 in units of 100 nanoseconds
*/
#define TIMESPEC_TO_FILETIME_OFFSET (((LONGLONG)27111902 << 32) + (LONGLONG)3577643008)


static void
filetime_to_timespec(const FILETIME *ft, struct timespec *ts)

/*-------------------------------------------------------------------
 * converts FILETIME (as set by GetSystemTimeAsFileTime), where  time is
 * expressed in 100 nanoseconds from Jan 1, 1601,
 * into struct timespec
 * where the time is expressed in seconds and nanoseconds from Jan 1, 1970.
 *-------------------------------------------------------------------
 * From: https://www.sourceware.org/ml/pthreads-win32/1999/msg00085.html
 */
{
    ts->tv_sec = (int)((*(LONGLONG *)ft - TIMESPEC_TO_FILETIME_OFFSET) /
        10000000);
    ts->tv_nsec = (int)((*(LONGLONG *)ft - TIMESPEC_TO_FILETIME_OFFSET -
        ((LONGLONG)ts->tv_sec * (LONGLONG)10000000)) * 100);
}


} // namespace

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
        : is_eof_{ false }, name_{ name } 
    {
        //TODO: Do we need to convert the stat? Can we use win32 data in stead?
    }

   /*! Constructor for a "fake" directoty iterator.
    *
    * This is used when we onlly want it to "iterate over" a single
    * entity (MLST). The path can point to a dir or a file.
    */
    FileListIterator(const Path& path, const boost::string_ref& name)
        : path_{ &path }, is_eof_{ false }, name_{ name } {}

    /*! Constructor for a real directory iterator */
    FileListIterator(const Path& path)
        : path_{&path}, ffw_{path.GetPhysPath()}, is_eof_{ffw_.IsEof()}
    {
        LOG_TRACE1_FN << "Opened directory for list: "
            << log::Esc(path.GetPhysPath().string());

        Capture();
    }

    ~FileListIterator() {
        Close();
    }

    void Close() {
        ffw_.Close();
    }

    void AddVirtualPath(const boost::string_ref& name, const struct stat& st) {
        vpaths_.push_back({ name.to_string(), st });
    }

    FileListIterator& operator ++() {
        Advance();
        return *this;
    }

    bool IsFake() const noexcept { return !ffw_.IsInitialized(); }

    // Intended only to detect eof
    bool operator != (const FileListIterator&) const {
        return !is_eof_ || have_vpath_;
    }

    /*! Get extended information (more than just the name) */
    void Stat() {}

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

    file_len_t GetSize() const noexcept { 
        const auto& d = ffw_.GetData();
        return (static_cast<file_len_t>(d.nFileSizeHigh) 
            * (static_cast<file_len_t>(MAXDWORD) + 1)) 
            + d.nFileSizeLow;
    }

    const timespec GetModifyTime() const noexcept {

        timespec ts{};
        detail::filetime_to_timespec(&ffw_.GetData().ftLastWriteTime, &ts);
        return ts;
    }

    bool IsDirectory() const noexcept { 
        return ffw_.GetData().dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    }

    const boost::string_ref& GetName() const noexcept {
        return name_;
    }

    const Path *GetPath() const noexcept { return path_; }

    boost::string_ref GetUniqueFact() const noexcept {
        if (path_ == nullptr) {
            return name_; // Pure virtual
        }
    
        // TODO: Implement - could use GetFileInformationByHandle() and
        //  dwVolumeSerialNumber + nFileIndexHigh + nFileIndexLow
        return {};
    }

private:
    void Advance() {
        if (IsFake()) {
            is_eof_ = true;
            return;
        }

        if (!is_eof_) {
            is_eof_ = !ffw_.Advance();
        }

        Capture();
    }

    void Capture()
    {
        if (!is_eof_) {
            name_ = boost::string_ref(ffw_.GetData().cFileName);
        } else {
            if (vpaths_.empty()) {
                name_.clear(); // Done
                have_vpath_ = false;
            }
            else {
                name_ = vpaths_.back().first;
                // TODO: Do we need to convert stat?
                //st_ = vpaths_.back().second;
                vpaths_.pop_back();
                have_vpath_ = true;
            }
        }
    }

    const Path *path_ = nullptr;
    detail::FindFileWrapper ffw_;
    bool is_eof_ = true;
    bool have_vpath_ = false;
    boost::string_ref name_;
    boost::filesystem::path stat_path_;
    mutable char unique_fact_buf[64]{};
    std::vector<std::pair<std::string, struct stat>> vpaths_;
};


} // namespace
