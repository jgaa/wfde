#include "war_wfde.h"
#include "WfdeAsciiFile.h"

#include <warlib/WarLog.h>
#include "wfde/ftp_protocol.h"
#include <warlib/error_handling.h>


using namespace std;
using namespace std::string_literals;

namespace war {
namespace wfde {
namespace impl {


WfdeAsciiFile::WfdeAsciiFile(unique_ptr< File > file)
: file_{std::move(file)}
{
    LOG_DEBUG_FN << "Wrapping " << *file_ << " for ASCII transfer";
}

WfdeAsciiFile::~WfdeAsciiFile()
{
}


/*! Convert from local format to CRLF delimited text */
File::const_buffer_t WfdeAsciiFile::Read(size_t bytes)
{
    auto buffer = file_->Read(bytes);
    buffer_.clear();

    //for(const auto& b : buffer) {
        auto p = static_cast<const char*>(buffer.data());
        const decltype(p) end = p + boost::asio::buffer_size(buffer);

        for(; p < end; ++p) {
            if (UNLIKELY(*p == '\r'))
                continue;
            if (UNLIKELY(*p == '\n')) {
                buffer_ += crlf_;
            } else {
                buffer_ += *p;
            }
        }
    //}

    return {buffer_.c_str(), buffer_.size()};
}

File::mutable_buffer_t WfdeAsciiFile::Write(const size_t bytes)
{
    buffer_.clear();
    buffer_.resize(std::min(bytes ? bytes : GetSegmentSize(),
                            GetSegmentSize()/2));

    return {&buffer_[0], buffer_.size()};
}

/*! Convert from CRLF formatted text to local text format */
void WfdeAsciiFile::SetBytesWritten(const size_t bytes)
{
    WAR_ASSERT(bytes <= buffer_.size());

    auto wr_buf = file_->Write(bytes * 2); //make space for worst-case expansion
    WAR_ASSERT(wr_buf.begin() + 1 == wr_buf.end()); // We don't want the complexity with several buffers

    auto wr_ptr = static_cast<char*>(wr_buf.data());
    auto b = wr_ptr;
    WAR_ASSERT(b != nullptr);

    auto ch = buffer_.cbegin();
    const auto end = ch + bytes;
    for(; ch != end; ++ch) {
        if (UNLIKELY(*ch == '\r'))
            continue;
        if (UNLIKELY(*ch == '\n')) {
            for(const auto eol : local_eol_) {
                *b++ = eol;
            }
        } else {
            *b++ = *ch;
        }
    }

    const auto bytes_written = b - wr_ptr;

#ifdef DEBUG
    LOG_TRACE4_F_FN(log::LA_IO) << "ASCII mapping: bytes=" << bytes
        << ", bytes_written=" << bytes_written
        << ' ' << *this;
#endif

	WAR_ASSERT(bytes_written <= static_cast<decltype(bytes_written)>(bytes * 2));

    file_->SetBytesWritten(bytes_written);
}


}}} // namespaces

