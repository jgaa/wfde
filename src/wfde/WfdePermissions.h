#pragma once

#include <string>

#include <wfde/wfde.h>
#include "wfde/multipaths.h"

namespace war {
namespace wfde {
namespace impl {

class WfdePermissions final : public Permissions
{
public:
    WfdePermissions() = default;
    ~WfdePermissions() = default;
    WfdePermissions(const WfdePermissions& v);

    const Path& GetPath(const Path::vpath_t& path,
                        Path::vpath_t *remaining = nullptr) const override;
    void AddPath(std::unique_ptr<Path>&& path) override;
    pathlist_t GetPaths() const override;
    void Merge(const Permissions& perms) override;
    ptr_t Copy() const override;
private:
    mpaths::paths_t paths_;
};


}}} // Namespaces

// Add som e
