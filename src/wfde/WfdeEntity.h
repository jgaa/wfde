#pragma once

#include <thread>
#include <regex>
#include <type_traits>
#include "wfde/wfde.h"
#include "log/WarLog.h"

namespace war {
namespace wfde {
namespace impl {

template <typename ParentT, typename T, typename ChildT>
class WfdeEntity : virtual public Entity
{
public:
    using ptr_t = std::shared_ptr<WfdeEntity>;
    using wptr_t = std::weak_ptr<WfdeEntity>;
    using parent_t = ParentT;
    using child_t = ChildT;

    WfdeEntity(parent_t* parent, const Configuration::ptr_t& conf)
    : name_{conf->GetValue("/Name", "Default")}, conf_{conf},  parent_{parent}
    {
    }

    ~WfdeEntity() {}

    std::string GetName() const override {
        WAR_ASSERT(!name_.empty());
        return name_;
    }

    parent_t *GetParent() const override {
        WAR_ASSERT(parent_);
        WAR_ASSERT(dynamic_cast<const Entity*>(parent_) != this);
        return parent_;
    }

    bool HaveParent() const override { return parent_; }

    void SetParent(Entity &parent) override {
        if (parent_) {
            WAR_THROW_T(ExceptionAlreadyExist,
                "Parent can only be set once");
        }
        WAR_POINTER_ASSERT(&parent, parent_t);
        parent_ = &dynamic_cast<parent_t &>(parent);
    }

    Threadpool& GetIoThreadpool() const override {
        return GetParent()->GetIoThreadpool();
    }

    children_t GetChildren(const std::string& filter = ".*") const {
        WAR_ASSERT(false && "Must be overridden by inherited class");
        return children_t();
    }

    void SetPermissions(const Permissions::ptr_t& perms) override {
        std::unique_lock<std::mutex> lock(mutex_);
        permissions_ = perms;
    }

    Permissions::ptr_t GetPermissions() const override {
        std::unique_lock<std::mutex> lock(mutex_);
        return permissions_;
    }

    Permissions::ptr_t GetEffectivePermissions() const override {

        auto permissions = GetPermissions();

        if (!permissions) {
            if (parent_)
                return parent_->GetEffectivePermissions();
            else
                WAR_THROW_T(ExceptionMissingInternalObject, "Permissions")
        }

        auto rval = permissions->Copy();

        if (Entity *parent = parent_) {
            for(; parent->HaveParent(); parent = parent_->GetParent()) {
                const auto pperms = parent->GetPermissions();
                if (pperms) {
                    rval->Merge(*pperms);
                }
            }
        }

        return rval;
    }

protected:
    template <typename retT = Entity::children_t>
    retT GetMyChildren(const std::string& filter,
        const std::map<std::string, std::shared_ptr<child_t>>& children) const {
        const std::regex pat{ filter };
        retT ret;
        {
            for(auto child : children) {
                if (regex_match(child.first, pat)) {
                    ret.push_back(child.second);
                }
            }
        }
        return ret;
    }

    template <typename retT = Entity::children_t>
    retT GetMyChildren(const std::string& filter,
        const std::vector<std::shared_ptr<child_t>>& children) const {
        const std::regex pat{ filter };
        retT ret;
        {
            std::copy_if(children.begin(), children.end(),
                std::back_inserter(ret),
                    [&pat](const std::shared_ptr<child_t>& ch) {
                    return regex_match(ch->GetName(), pat);
                });
        }
        return ret;
    }

    const std::string name_;
    const Configuration::ptr_t conf_;
    parent_t *parent_ = nullptr;
    Permissions::ptr_t permissions_;
    mutable std::mutex mutex_;
};

} // namespace impl
} // namespace wfde
} // namespace war
