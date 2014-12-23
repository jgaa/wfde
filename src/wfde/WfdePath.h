#pragma once

#include <string>

#include "wfde/wfde.h"

namespace war {
namespace wfde {
namespace impl {

class WfdePath : public Path
{
public:
    WfdePath(const vpath_t& vpath,
             const ppath_t& ppath,
             const permbits_t permbits,
             const Type& type = Type::DIRECTORY);

    WfdePath(const WfdePath& v);

    const vpath_t& GetVirtualPath() const override { return vpath_; }
    const ppath_t& GetPhysPath() const override { return ppath_; }
    boost::string_ref GetVpathFileName() const override;
    bool CanRead() const noexcept override {
        return Bit(PermissionBits::CAN_READ) & permbits_;
    }
    bool CanWrite() const noexcept override {
        return Bit(PermissionBits::CAN_WRITE) & permbits_;
    }
    bool CanExecute() const noexcept override {
        return Bit(PermissionBits::CAN_EXECUTE) & permbits_;
    }
    bool CanEnter() const noexcept override {
        return Bit(PermissionBits::CAN_ENTER) & permbits_;
    }
    bool CanList() const noexcept override {
        return Bit(PermissionBits::CAN_LIST) & permbits_;
    }
    bool CanCreateDir() const noexcept override {
        return Bit(PermissionBits::CAN_CREATE_DIR) & permbits_;
    }
    bool CanCreateFile() const noexcept override {
        return Bit(PermissionBits::CAN_CREATE_FILE) & permbits_;
    }
    bool CanDeleteFile() const noexcept override {
        return Bit(PermissionBits::CAN_DELETE_FILE) & permbits_;
    }
    bool CanDeleteDir() const noexcept override {
        return Bit(PermissionBits::CAN_DELETE_DIR) & permbits_;
    }
    bool CanSeeHiddenFiles() const noexcept override {
        return Bit(PermissionBits::CAN_SEE_HIDDEN_FILES) & permbits_;
    }
    bool CanSeeHiddenDirs() const noexcept override {
        return Bit(PermissionBits::CAN_SEE_HIDDEN_DIRS) & permbits_;
    }
    bool CanCreateHiddenFile() const noexcept override {
        return Bit(PermissionBits::CAN_CREATE_HIDDEN_FILES) & permbits_;
    }
    bool CanCreateHiddenDir() const noexcept override {
        return Bit(PermissionBits::CAN_CREATE_HIDDEN_DIRS) & permbits_;
    }
    bool CanSetTimestamp() const noexcept override {
        return Bit(PermissionBits::CAN_SET_TIMESTAMP) & permbits_;
    }
    bool CanSetPermissions() const noexcept override {
        return Bit(PermissionBits::CAN_SET_PERMISSIONS) & permbits_;
    }
    bool CanRename() const noexcept override {
        return Bit(PermissionBits::CAN_RENAME) & permbits_;
    }
    bool IsRecursive() const noexcept override {
        return Bit(PermissionBits::IS_RECURSIVE) & permbits_;
    }
    bool IsSharedUploadDir() const noexcept override {
        return Bit(PermissionBits::IS_SHARED_UPLOAD_DIR) & permbits_;
    }
    Type GetType() const override { return type_; }
    bool Exists() const override;
    bool IsSameParentDir(const Path& path) override;
    std::unique_ptr<Path> Copy() const override;
    std::unique_ptr<Path> CreateSubpath(const vpath_t& subPath,
                                        Type pathType) const override;
    std::unique_ptr<Path> Copy(permbits_t bits) const override;

    static permbits_t Bit(const PermissionBits bit) noexcept {
        return static_cast<permbits_t>(bit);
    }

    static std::vector<boost::string_ref> DoSplit(const std::string& partsToSplit,
                                                  char splitCh = '/');

    static permbits_t ToPermBit(const boost::string_ref& name);
private:
    /*! Prefixes the vpath with '/' if it is missing */
    static Path::vpath_t MakeValid(const Path::vpath_t& vpath);

    const ppath_t ppath_;
    const vpath_t vpath_;
    const permbits_t permbits_;
    const Type type_;
};


}}} // Namespaces

