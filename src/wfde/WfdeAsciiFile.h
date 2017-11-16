#pragma once

#include <warlib/config.h>
#include "WfdeFile.h"

namespace war {
namespace wfde {
namespace impl {

/*! Wrapper around a File to map EOL to and from CRLF
 *
 * Required by the FTP ASCII Transfer Type
 */
class WfdeAsciiFile : public File
{
public:
    WfdeAsciiFile(std::unique_ptr<File> file);
    ~WfdeAsciiFile();

    const_buffer_t Read(std::size_t bytes) override;
    mutable_buffer_t Write(size_t bytes = 0) override;
    void SetBytesWritten(size_t bytes) override;
    void Seek(fpos_t pos) override {file_->Seek(pos);};
    fpos_t GetPos() const override { return file_->GetPos(); }
    fpos_t GetSize() const override { return file_->GetSize(); }
    bool IsEof() const override { return file_->IsEof(); }
    void Close() override { file_->Close(); }
    FileOperation GetOperation() const override { return file_->GetOperation(); }
    const boost::uuids::uuid& GetUuid() const override { return file_->GetUuid(); }
    std::size_t GetSegmentSize() const noexcept override {
        return file_->GetSegmentSize();
    }

private:
    std::unique_ptr<File> file_;
    std::string buffer_;
    const std::string crlf_ = "\r\n";
    const std::string local_eol_ = WAR_SYSTEM_EOL;
};

}}} // namespaces
