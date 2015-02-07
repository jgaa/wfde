
#include "war_wfde.h"
#include "WfdeFile.h"
#include "war_uuid.h"
#include "log/WarLog.h"
#include "wfde/ftp_protocol.h"
#include "war_error_handling.h"

using namespace std;
using namespace std::string_literals;

std::ostream& operator << (std::ostream& o, const war::wfde::File& f) {

    return o << "{File " << boost::uuids::to_string(f.GetUuid()) << '}';
}

std::ostream& operator << (std::ostream& o,
                           const war::wfde::File::FileOperation& op) {

    static const vector<std::string> names = {
        "READ"s, "WRITE"s, "WRITE_NEW"s, "APPEND"s
    };

    WAR_ASSERT(static_cast<decltype(names.size())>(op) < names.size());

    return o << names[static_cast<int>(op)];
}


namespace war {
namespace wfde {
namespace impl {


WfdeFile::WfdeFile(const boost::filesystem::path& path, FileOperation operation)
: path_{path}
, operation_{operation}
, id_(boost::uuids::random_generator()())
{
    bool must_exist = true;
    bool truncate_if_exists = false;

    const auto st = boost::filesystem::status(path_);

    switch(operation) {
        case FileOperation::READ:
            WAR_ASSERT(mode_ == boost::interprocess::read_only);
            WAR_ASSERT(must_exist == true);
        break;
        case FileOperation::WRITE_NEW:
            if (boost::filesystem::exists(st)) {
                WAR_THROW_T(ExceptionAlreadyExist, path_.string());
            }
            // Fall trough
        case FileOperation::APPEND:
        case FileOperation::WRITE:
            mode_ = boost::interprocess::read_write;
            must_exist = false;
            truncate_if_exists = operation != FileOperation::APPEND;
        break;
    default:
        WAR_ASSERT(false && "Not implemented");
        break;
    }

    if (must_exist) {
        if (!boost::filesystem::is_regular_file(st)) {
            LOG_NOTICE_FN << "The path " << log::Esc(path.string())
                << " is not a regular file";
            WAR_THROW_T(ExceptionNotFound, "Not a file");
        }
    } else {
        if (boost::filesystem::exists(st)) {
            if (truncate_if_exists) {
                boost::filesystem::resize_file(path_, 0);
            }
        } else {
            // Just create an empty file
            std::ofstream touch(path_.c_str());
        }
    }

    end_of_file_pos_ = file_size_ = boost::filesystem::file_size(path_);
    MapFile();

    if (operation == FileOperation::APPEND) {
        Seek(end_of_file_pos_);
    }
}

WfdeFile::~WfdeFile()
{
    Close();
}

File::const_buffer_t WfdeFile::Read(std::size_t bytes)
{
    auto b = GetBufferValues(bytes);
    return { static_cast<const char *>(region_.get_address()) + b.first, b.second};

}

File::mutable_buffer_t WfdeFile::Write(size_t bytes)
{
    if (!bytes)
        bytes = segment_size_;

    auto min_file_size = pos_ + bytes + segment_size_;
    if (min_file_size > GetSize()) {

        auto align = (min_file_size % segment_size_) ? segment_size_ : 0;
        auto segments = (min_file_size / segment_size_);
        min_file_size = std::max((segments * segment_size_) + align, GetSize() + grow_size_);

#ifdef DEBUG
        LOG_TRACE4_F_FN(log::LA_IO) << "Enlarging the file " << *this
            << ' ' << log::Esc(path_.string()) << ' '
            << " from " << GetSize() << " to " << min_file_size << " bytes";
#endif

#ifdef WIN32
        UnmapFile();
#endif
        boost::filesystem::resize_file(path_, min_file_size);
        file_size_ = min_file_size;
    }

    auto b = GetBufferValues(bytes);

#ifdef DEBUG
    LOG_TRACE4_F_FN(log::LA_IO) << "bytes=" << bytes
        << ", last_buffer_len_=" << last_buffer_len_
        << ", buffer-size=" << b.second
        << ' ' << *this;
#endif

    WAR_ASSERT(bytes <= b.second);
    return { static_cast<char *>(region_.get_address()) + b.first, b.second};
}

void WfdeFile::SetBytesWritten(size_t bytes)
{
#ifdef DEBUG
    LOG_TRACE4_F_FN(log::LA_IO) << "bytes=" << bytes
        << ", last_buffer_len_=" << last_buffer_len_
        << *this;
#endif

    WAR_ASSERT(bytes <= last_buffer_len_);

    if (bytes < last_buffer_len_) {
        auto diff = last_buffer_len_ - bytes;
        pos_ -= diff;
        do_truncate_ = true;
    }

    if (end_of_file_pos_ < pos_)
        end_of_file_pos_ = pos_;
}

pair< File::fpos_t, size_t > WfdeFile::GetBufferValues(const std::size_t bytes)
{
    MapRegion(bytes);

    const size_t offset = pos_ - region_start_ ;
    const size_t seg_len = region_.get_size() - offset;
    const size_t use_len = bytes ? std::min(bytes, seg_len) : seg_len;

    pos_ += use_len;

    WAR_ASSERT(region_.get_size() >= use_len);
    WAR_ASSERT(region_.get_address() && "Can not be nullptr");

#ifdef DEBUG
#define VAL(v) v << hex << "(0x" << v << ')' << dec
    LOG_TRACE4_F_FN(log::LA_IO)
        << "Returning buffer region: pos_=" << VAL(pos_)
        << ", offset=" << VAL(offset)
        << ", use_len=" << VAL(use_len)
        << ", region_.get_address()=" << region_.get_address()
        << ", [requested]bytes=" << bytes
        << ", use_len=" << use_len;
#undef VAL
#endif
    last_buffer_len_ = use_len;

    return {offset, use_len};
}


void WfdeFile::MapRegion(const std::size_t wantBytes)
{
    if (!have_mapped_file_) {
        MapFile();
    }
    const auto file_size = GetSize();
    auto start_segment = pos_ / segment_size_;
    region_start_ = start_segment * segment_size_;
    const auto max_region_size = wantBytes
        ? std::min(wantBytes + segment_size_, region_limit_) : region_limit_;
    const auto region_len = min(max_region_size, file_size - region_start_);

    region_ = boost::interprocess::mapped_region(file_, mode_, region_start_,
                                                 region_len);
    have_mapped_region_ = true;
    region_.advise(boost::interprocess::mapped_region::advice_sequential);

#ifdef DEBUG
#define VAL(v) v << hex << "(0x" << v << ')' << dec
    LOG_TRACE4_F_FN(log::LA_IO) << "Mapping file " << *this
        << " pos_=" << VAL(pos_)
        << ", region_start_=" << VAL(region_start_)
        << ", region_len=" << VAL(region_len)
        << ", region_limit_=" << VAL(region_limit_)
        << ", file_size=" << VAL(file_size)
        << ", start_segment=" << VAL(start_segment)
        << ", [ofs from ptr]=" << VAL(pos_ - region_start_);
#undef VAL
#endif
}

void WfdeFile::Seek(File::fpos_t pos)
{
    const auto len = GetSize();
    if (pos > len) {
        LOG_NOTICE_FN << "Seek beoind EOF, file-size=" << len
            << ", requested position=" << pos
            << ' ' << *this;
        WAR_THROW_T(ExceptionSeekBeoindEof, boost::uuids::to_string(id_));
    }

    LOG_TRACE4_F_FN(log::LA_IO) << "Setting pos to " << pos << " in " << *this;
    pos_ = pos;
}

void WfdeFile::Close()
{
    if (!closed_) {
        closed_ = false;
        LOG_TRACE2_FN << "Closing " << *this;
        UnmapFile();

        if (do_truncate_
            && (file_size_ > end_of_file_pos_)
            && boost::filesystem::is_regular(path_)) {
            WAR_ASSERT(end_of_file_pos_ >= 0);
            LOG_TRACE4_F_FN(log::LA_IO) << "Truncating file " << *this
                << " from " << file_size_
                << " to " << end_of_file_pos_ << " bytes";

            try {
                resize_file(path_, end_of_file_pos_);
            } WAR_CATCH_ERROR;
        }
    }
}


}}} // namespaces
