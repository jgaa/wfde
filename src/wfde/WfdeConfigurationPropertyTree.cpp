#include "war_wfde.h"

#include <boost/property_tree/info_parser.hpp>

#include "WfdeConfigurationPropertyTree.h"

using namespace std;

namespace war {
namespace wfde {
namespace impl {


WfdeConfigurationPropertyTree::WfdeConfigurationPropertyTree(const string& path)
: path_{path}
{
    // Read the configuration.
    if (boost::filesystem::exists(path)) {
        boost::property_tree::read_info(path, data_);
    } else {
        LOG_ERROR << "File " << log::Esc(path) << " don't exist.";
        WAR_THROW_T(ExceptionNotFound, path);
    }
}

string WfdeConfigurationPropertyTree::GetValue(const char* path, const char* defaultVal) const
{
    WAR_ASSERT(path);
    return data_.get(ToPath(path), defaultVal ? defaultVal : "");
}

bool WfdeConfigurationPropertyTree::HaveValue(const char* path) const
{
    WAR_ASSERT(path);
    return data_.get_child_optional(ToPath(path)).is_initialized();
}

void WfdeConfigurationPropertyTree::SetValue(const char* path, string value)
{
    WAR_ASSERT(path);
    data_.put(ToPath(path), value);
}

Configuration::node_enum_t WfdeConfigurationPropertyTree::EnumNodes(const char* path)
{
    Configuration::node_enum_t rval;

    WAR_ASSERT(path);

    try {
        const auto sub = data_.get_child(ToPath(path));

        for(const auto& v : sub) {
            rval.push_back({v.first});
        }
    } catch(boost::property_tree::ptree_bad_path&) {
        ; // Do nothing
    }

    return rval;
}

boost::property_tree::path WfdeConfigurationPropertyTree::ToPath(const char* path) const
{
    // Strip off leading slash
    if (*path == '/') {
        ++path;
    }

    return boost::property_tree::path(path, '/');
}

} // namespace impl

Configuration::ptr_t Configuration::GetConfiguration(const std::string& path) {

    LOG_NOTICE << "Reading configuration-file: " << log::Esc(path);

    return impl::WfdeConfigurationPropertyTree::CreateInstance(path);
}

Configuration::ptr_t Configuration::CreateInstance() {

    return impl::WfdeConfigurationPropertyTree::CreateInstance();
}

}} // namespace
