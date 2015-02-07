#pragma once

#include <string>

#include <boost/filesystem.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include "wfde/wfde.h"

namespace war {
namespace wfde {
namespace impl {

class WfdeFile : public File
{
public:
    WfdeFile(const boost::filesystem::path& path, FileOperation operation);
    ~WfdeFile();

    struct ExceptionSeekBeoindEof : public ExceptionBase {};

    const_buffer_t Read(std::size_t bytes) override;
    mutable_buffer_t Write(size_t bytes = 0) override;
    void SetBytesWritten(size_t bytes) override;
    void Seek(fpos_t pos) override;
    fpos_t GetPos() const override { return pos_; }
    fpos_t GetSize() const override { return file_size_; }
    bool IsEof() const override { return pos_ == GetSize(); }
    void Close() override;
    FileOperation GetOperation() const override { return operation_; }
    const boost::uuids::uuid& GetUuid() const override { return id_; }
    std::size_t GetSegmentSize() const noexcept override {
        return segment_size_;
    }

private:
    void MapRegion(const std::size_t wantBytes); // Map the region_ according to pos_
    std::pair<std::size_t, std::size_t> GetBufferValues(const std::size_t bytes);
    void MapFile() {
        UmapRegion();
        file_ = boost::interprocess::file_mapping(path_.string().c_str(), mode_);
        have_mapped_file_ = true;
    }

    void UmapRegion() {
        if (have_mapped_region_) {
            region_ = boost::interprocess::mapped_region();
            have_mapped_region_ = false;
        }
    }

    void UnmapFile() {
        if (have_mapped_file_) {
            UmapRegion();
            file_ = boost::interprocess::file_mapping();
            have_mapped_file_ = false;
        }
    }

    fpos_t pos_ = 0;
    const boost::filesystem::path path_;
    const FileOperation operation_;
    const boost::uuids::uuid id_;
    boost::interprocess::file_mapping file_;
    boost::interprocess::mapped_region region_;
    boost::interprocess::mode_t mode_ = boost::interprocess::read_only;
    const std::size_t segment_size_ = region_.get_page_size();
    const std::size_t region_limit_ = (segment_size_ * 8);
    const std::size_t grow_size_ = region_limit_ * 128;
    fpos_t region_start_ = 0;
    fpos_t file_size_ = 0; // Actual length of file
    fpos_t end_of_file_pos_ = -1; // The "real" eof, determined by initial
        // file size, and the bytes we have actually appended to this.
    std::size_t last_buffer_len_ = 0; // The length of the last write buffer we returned
    bool do_truncate_ = false; // Adjust the file-size to end_of_file_pos_ when
        // we close the file.
    bool closed_ = false;
    bool have_mapped_region_ = false;
    bool have_mapped_file_ = false;
};


}}} // namespaces

