#pragma once

#include <boost/property_tree/ptree.hpp>
#include <wfde/wfde.h>

namespace war {
namespace wfde {
namespace impl {

/*! This class implements the Configuration interface
 *
 * using boost::propetry_tree as it's base.
 *
 */
class WfdeConfigurationPropertyTree : public Configuration,
public std::enable_shared_from_this<WfdeConfigurationPropertyTree>
{
public:
    class SubConfig : public Configuration
    {
    public:
        SubConfig(std::string root, Configuration::ptr_t conf)
            : root_path_(std::move(root)), conf_(std::move(conf))
        {}

        Configuration::ptr_t GetConfigForPath(std::string path) override {
            return std::make_shared<SubConfig>(
                root_path_ + Sep(path.c_str()) + path, conf_);
        }

        std::string GetValue(const char* path,
                             const char *defaultVal) const override {
            return conf_->GetValue((root_path_ + Sep(path) + path).c_str(),
                                   defaultVal);
        }
        
        bool HaveValue(const char* path) const override {
            return conf_->HaveValue(path);
        }

        bool CanWrite(const char* path) override {
            return conf_->CanWrite();
        }

        void SetValue(const char* path, std::string value) override {
            conf_->SetValue((root_path_ + Sep(path) + path).c_str(), value);
        }

        node_enum_t EnumNodes(const char *path) override {
            return conf_->EnumNodes((root_path_ + Sep(path) + path).c_str());
        }

    private:
        const std::string& Sep(const char *path) const noexcept {
            const static std::string empty, slash("/");

            WAR_ASSERT(path != nullptr);
            if (path && (*path == '/'))
                return empty;
            return slash;
        }

        const std::string root_path_;
        ptr_t conf_;
    };


    WfdeConfigurationPropertyTree(const WfdeConfigurationPropertyTree&) = delete;
    WfdeConfigurationPropertyTree& operator = (const WfdeConfigurationPropertyTree&) = delete;

    ptr_t GetConfigForPath(std::string path) override {
        return std::make_shared<SubConfig>(path, shared_from_this());
    }

    std::string GetValue(const char* path, const char* defaultVal = nullptr) const override;
    
    bool HaveValue(const char* path) const override;

    bool CanWrite(const char* path) override { return true; }

    void SetValue(const char* path, std::string value) override;

    node_enum_t EnumNodes(const char* path) override;

    static auto CreateInstance(const std::string& path) {
        // Must use new, since make_shared don't has access to the
        // private constructor.
        return std::shared_ptr<WfdeConfigurationPropertyTree>(
            new WfdeConfigurationPropertyTree(path));
    }
    
    static auto CreateInstance() {
        // Must use new, since make_shared don't has access to the
        // private constructor.
        return std::shared_ptr<WfdeConfigurationPropertyTree>(
            new WfdeConfigurationPropertyTree());
    }

private:
    // Prevent construction on the stack
    WfdeConfigurationPropertyTree(const std::string& path);
    WfdeConfigurationPropertyTree() = default;
    boost::property_tree::path ToPath(const char *path) const;
    boost::property_tree::ptree data_;
    const std::string path_;
};

}}} // namespaces
