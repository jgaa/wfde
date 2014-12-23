#pragma once

#include <boost/filesystem.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include "wfde/wfde.h"
#include "wfde/file_listing.h"
#include "war_error_handling.h"
#include "war_uuid.h"
#include "log/WarLog.h"
#include "wfde/ftp_protocol.h"

/* I don't like using namespaces in header-files, but this makes sense
 * as this header-file is actually an implementation detail used by
 * wfde_ftp_commands.cpp only, and the using simplifies this file a lot.
 */
using namespace war::wfde::file_listing;

namespace war {
namespace wfde {
namespace impl {

class LsLongFormat
{
public:
    LsLongFormat(const FtpState& /*state*/)
    : year_time_border_{time(0) - half_year()} {}

    int CalculateSize(const FileListIterator& fli) const noexcept {

        return approx_size_ = (fli.GetName().size() + 64);
    }

    int PrintIt(char *buffer, const FileListIterator& fli) const noexcept {

        // Long format
        auto cur = buffer;

        // ls -l format is outdated and obsolete. Let's not waste
        // CPU on simulating more than we absolutely need to do,

        static const char bs_str_file[] = {"-rw-r--r-- 1 ftp ftp "};
        static const char bs_str_dir[] = {"drwxr-xr-x 1 ftp ftp "};
        constexpr auto bs_str_len = sizeof(bs_str_file) -1;
        static_assert(bs_str_len == (sizeof(bs_str_dir) -1),
                        "BS strings are different size");

        const auto is_dir = fli.IsDirectory();
        const char *bs_string = is_dir ? bs_str_dir : bs_str_file;
        memcpy(cur, bs_string, bs_str_len);
        cur += bs_str_len;

        if (!is_dir) {
            cur += snprintf(cur, 16, "%8lld ",
                static_cast<long long>(fli.GetSize()));
        } else {
            memcpy(cur, "       1 ", 9);
            cur += 9;
        }

        static const char* months[] {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

        struct tm tm{};
        if (UNLIKELY(!gmtime_r(&fli.GetModifyTime().tv_sec, &tm))) {
            tm.tm_mday = 1; // Just fix it so we get a valid listing
        }

        memcpy(cur, months[tm.tm_mon], 3);
        cur += 3;

        if ((fli.GetModifyTime().tv_sec) > year_time_border_) {
            // Print time
            cur += snprintf(cur, 16, " %2d %2d:%02d ",
                            static_cast<int>(tm.tm_mday),
                            static_cast<int>(tm.tm_min),
                            static_cast<int>(tm.tm_sec));
        } else {
            // Print year
            cur += snprintf(cur, 16, " %2d  %d ",
                            static_cast<int>(tm.tm_mday),
                            static_cast<int>(tm.tm_year + 1900));
        }

        const auto name_len = fli->GetName().size();
        memcpy(cur, fli.GetName().data(), name_len);
        cur += name_len;
        *cur++ = '\r';
        *cur++ = '\n';

        return cur - buffer;
    }

private:
    mutable int approx_size_{};
    static constexpr time_t half_year() { return 60*60*24*(163/2); }
    const time_t year_time_border_;
};

class NlstFormat
{
public:
    NlstFormat(const FtpState& /*state*/) {}

    int CalculateSize(const FileListIterator& fli) const noexcept {

        return fli.GetName().size() + 2;
    }

    int PrintIt(char *buffer, const FileListIterator& fli) const noexcept {
        auto cur = buffer;

        const auto name_len = fli->GetName().size();
        memcpy(cur, fli.GetName().data(), name_len);
        cur += name_len;

        *cur++ = '\r';
        *cur++ = '\n';

        return cur - buffer;
    }
};

void print_timeval(char *&cur, const timespec& ts);

class MlsdFormat
{
public:
    MlsdFormat(const FtpState& state)
    : state_{state.mdtx_state} {}

    int CalculateSize(const FileListIterator& fli) const noexcept {

        return approx_size_ = (fli.GetName().size() + 128);
    }

    int PrintIt(char *buffer, const FileListIterator& fli) const noexcept {

        static std::string cdir("cdir"), file("file"), dir("dir"), pdir("pdir"),
            name_cdir("."), name_pdir("..");

        // Long format
        cur_ = buffer;

        // Leading space
        *cur_++ = ' ';

        if (state_.IsEnabled(MdtxState::Facts::TYPE)) {
            Print(state_.GetFactName(MdtxState::Facts::TYPE));
            *cur_++ = '=';

            if (fli->IsDirectory()) {
                if (fli.GetName()[0] == '.') {
                    if (fli.GetName() == name_cdir)
                        Print(cdir);
                    else if (fli.GetName() == name_pdir) {
                        Print(pdir);
                    } else {
                        Print(dir);
                    }
                } else {
                    Print(dir);
                }
            } else {
                Print(file);
            }
            *cur_++ = ';';
        }

        if (state_.IsEnabled(MdtxState::Facts::MODIFY)) {
            Print(state_.GetFactName(MdtxState::Facts::MODIFY));
            *cur_++ = '=';
            print_timeval(cur_, fli.GetModifyTime());
            *cur_++ = ';';
        }

        if (state_.IsEnabled(MdtxState::Facts::SIZE)) {
            Print(state_.GetFactName(MdtxState::Facts::SIZE));
            *cur_++ = '=';
            cur_ += snprintf(cur_, 16, "%lld",
                static_cast<long long>(fli.GetSize()));
            *cur_++ = ';';
        }

        if (state_.IsEnabled(MdtxState::Facts::UNIQUE)) {
            Print(state_.GetFactName(MdtxState::Facts::UNIQUE));
            *cur_++ = '=';
            Print(fli.GetUniqueFact());
            *cur_++ = ';';
        }

        if (state_.IsEnabled(MdtxState::Facts::PERM)) {
            Print(state_.GetFactName(MdtxState::Facts::PERM));
            *cur_++ = '=';

            const auto path = fli.GetPath();

            if (fli.IsDirectory()) {

                if (path) {
                    if (path->CanCreateFile())
                        *cur_++ = 'c';
                    if (path->CanDeleteDir())
                        *cur_++ = 'd';
                    if (path->CanEnter())
                        *cur_++ = 'e';
                    if (path->CanCreateDir())
                        *cur_++ = 'm';
                    if ((path->CanDeleteFile() || path->CanDeleteDir()))
                        *cur_++ = 'p';
                } else {
                    // Assumed permissions on pure virtual folders
                    *cur_++ = 'e';
                }
            } else {
                // File
                WAR_ASSERT(path);

                if (path->CanWrite())
                    *cur_++ = 'a';
                if (path->CanDeleteFile())
                    *cur_++ = 'd';
                if (path->CanRead())
                    *cur_++ = 'r';
                if (path->CanWrite())
                    *cur_++ = 'w';
            }

            if (path && path->CanRename())
                    *cur_++ = 'f';

            *cur_++ = ';';
        }

        // Space before name, if we have facts (not a double space)
        // TODO: Double check RFC 3659 for this
        if (*(cur_ -1) != ' ')
            *cur_++ = ' ';

        Print(fli.GetName());

        // Newline
        *cur_++ = '\r';
        *cur_++ = '\n';

        WAR_ASSERT((cur_ - buffer) <= approx_size_);
        return cur_ - buffer;
    }

private:
    void Print(const boost::string_ref& str) const {
        memcpy(cur_, str.data(), str.size());
        cur_ += str.size();
    }

    void Print(const std::string& str) const {
        memcpy(cur_, str.c_str(), str.size());
        cur_ += str.size();
    }

    mutable char *cur_ = nullptr;
    mutable int approx_size_{};;
    const MdtxState& state_;
};

class DirListerLs : public DirLister<LsLongFormat>
{
public:
    DirListerLs(const Path& path, Session& session, const FtpState& state)
    : DirLister(path, session, state)
    {}
};

class DirListerNlst : public DirLister<NlstFormat>
{
public:
    DirListerNlst(const Path& path, Session& session, const FtpState& state)
    : DirLister(path, session, state)
    {}
};

class DirListerMlsd : public DirLister<MlsdFormat>
{
public:
    DirListerMlsd(const Path& path, Session& session, const FtpState& state)
    : DirLister(path, session, state) {}
};

class DirListerMlst : public DirLister<MlsdFormat>
{
public:
    DirListerMlst(const Path& path, Session& session, const FtpState& state)
    : DirLister(path, session, state, true) {}
};

template <typename DirListerT>
class WfdeFtpList : public File
{
public:
    using dir_lister_t = DirListerT;

    WfdeFtpList(std::unique_ptr<Path>&& path, Session& session,
                const FtpState& state)
    : id_(boost::uuids::random_generator()())
    , current_path_{move(path)}
    , dir_lister_{*current_path_, session, state}
    {}

    ~WfdeFtpList() {}

    const_buffer_t Read(std::size_t bytes) override {
        is_eof_ = (dir_lister_.List() == false);
        pos_ += dir_lister_.GetBytesUsed();

        LOG_TRACE4_FN << "Returning buffer with " << dir_lister_.GetBytesUsed()
            << " bytes. pos_=" << pos_ << ", is_eof_=" << is_eof_;

        return dir_lister_.GetBuffer();
    }

    mutable_buffer_t Write(size_t bytes = 0) {
        WAR_ASSERT(false);
        return {nullptr, 0};
    };
    void SetBytesWritten(size_t bytes) override { WAR_ASSERT(false); };
    void Seek(fpos_t pos) override { WAR_ASSERT(false); };;
    fpos_t GetPos() const override { return pos_; }
    fpos_t GetSize() const override { return 0; }
    bool IsEof() const override { return is_eof_; }

    void Close() override {
        dir_lister_.Close();
    }

    FileOperation GetOperation() const override { return FileOperation::READ; }
    const boost::uuids::uuid& GetUuid() const override { return id_; }

    std::size_t GetSegmentSize() const noexcept override {
        WAR_ASSERT(false);
        return 1024 * 16;
    }

private:
    const boost::uuids::uuid id_;
    std::unique_ptr<Path> current_path_;
    bool is_eof_ = false;
    fpos_t pos_ = 0;
    dir_lister_t dir_lister_;
};

std::string GetRfc3659FileTime(const Path& path);

std::string GetMlstFacts(const Path& path, Session& session,
                                const FtpState& state);

}}} // namespaces
