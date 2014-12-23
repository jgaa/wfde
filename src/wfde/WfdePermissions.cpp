#include "war_wfde.h"
#include "WfdePermissions.h"
#include "WfdePath.h"

#include "log/WarLog.h"
#include "war_error_handling.h"

using namespace std;
using namespace std::string_literals;

std::ostream& operator << (std::ostream& o, const war::wfde::Permissions& op) {

    o << " {Permissions \n{Paths:";

    for(const auto p : op.GetPaths()) {
        o << "\n {" << war::log::Esc(p->GetVirtualPath())
            << " --> " << war::log::Esc(p->GetPhysPath().string())
            << ": "
            << (p->CanRead() ? "CAN_READ " : "")
            << (p->CanWrite() ? "CAN_WRITE " : "")
            << (p->CanExecute() ? "CAN_EXECUTE " : "")
            << (p->CanEnter() ? "CAN_ENTER " : "")
            << (p->CanList() ? "CAN_LIST " : "")
            << (p->CanCreateDir() ? "CAN_CREATE_DIR " : "")
            << (p->CanDeleteDir() ? "CAN_DELETE_DIR " : "")
            << (p->CanCreateFile() ? "CAN_CREATE_FILE " : "")
            << (p->CanDeleteFile() ? "CAN_DELETE_FILE " : "")
            << (p->CanSeeHiddenFiles() ? "CAN_SEE_HIDDEN_FILES " : "")
            << (p->CanSeeHiddenDirs() ? "CAN_SEE_HIDDEN_DIRS " : "")
            << (p->CanCreateHiddenFile() ? "CAN_CREATE_HIDDEN_FILE " : "")
            << (p->CanCreateHiddenDir() ? "CAN_CREATE_HIDDEN_DIR " : "")
            << (p->CanSetTimestamp() ? "CAN_SET_TIMESTAMP " : "")
            << (p->CanSetPermissions() ? "CAN_SET_PERMISSIONS " : "")
            << (p->CanRename() ? "CAN_RENAME " : "")
            << (p->IsRecursive() ? "IS_RECURSIVE " : "")
            << (p->IsSharedUploadDir() ? "IS_SHARED_UPLOAD_DIR" : "")
            << "}"
            ;
    }
    return o << "}\n}";
}

namespace war {
namespace wfde {
namespace impl {


WfdePermissions::WfdePermissions(const WfdePermissions& v)
{
    for(const auto& path: v.paths_) {
        paths_.push_back(move(path->Copy()));
    }
}


const Path& WfdePermissions::GetPath(const Path::vpath_t& vpath,
                                     Path::vpath_t *remaining) const
{
    // The path must be normalized
    WAR_ASSERT(vpath.find("../") == vpath.npos);

    // Only the rooot-path "/" can end with a slash
    WAR_ASSERT((vpath.size() == 1) || (vpath.size() > 1
                                       && (vpath.back() != '/')));

    // Find vpath, or the longest mathing recursive parent path
    Path *best_match = nullptr;
    const auto use_path_end = vpath.cend();
    const auto use_path_begin = vpath.cbegin();
    auto remaining_it = vpath.cend();

    for(auto const &path : paths_.get<mpaths::tag_vpath>()) {
        const auto& curr_vpath = path->GetVirtualPath();

        LOG_TRACE4_FN << "vpath=" << log::Esc(vpath)
            << ", curr_vpath=" << log::Esc(curr_vpath);

        auto p = curr_vpath.cbegin();
        const auto p_end = curr_vpath.cend();
        auto pp = vpath.cbegin();

        for(; (p_end != p) && (*p == *pp); ++p, ++pp) {
            ; // step
        }

        if (p == p_end) {
            // Possible match
            if (pp == use_path_end) {
                // Full match
                if (remaining) {
                    remaining->empty();
                }
                return *path.get();
            } else if (use_path_begin != pp)  {
                if (p == p_end) {
                    if (path->IsRecursive()) {
                        best_match = path.get();
                        remaining_it = pp;
                        // Skip '/'
                        while ((remaining_it != use_path_end) && (*remaining_it == '/'))
                            ++remaining_it;
                    } else {
                        best_match = nullptr;
                    }
                }
            }
        }
    }

    if (!best_match) {
        LOG_TRACE1_FN << "Unable to resolve vpath: " << log::Esc(vpath);
        WAR_THROW_T(ExceptionAccessDenied, vpath);
    }

    if (remaining) {
        remaining->assign(remaining_it, use_path_end);
    }
    return *best_match;
}

void WfdePermissions::AddPath(unique_ptr< Path >&& path)
{
    // Check if the paths exists
    {
        auto& vpix = paths_.get<mpaths::tag_vpath>();
        if (vpix.find(path->GetVirtualPath()) != vpix.end()) {
            LOG_TRACE2_FN << "The vpath " << log::Esc(path->GetVirtualPath())
                << " already exists in permissions ";
            WAR_THROW_T(ExceptionAlreadyExist, "vpath");
        }
    }

    {
        auto& ppix = paths_.get<mpaths::tag_ppath>();
        if (ppix.find(path->GetPhysPath()) != ppix.end()) {
            LOG_TRACE2_FN << "The ppath " << log::Esc(path->GetPhysPath().string())
                << " already exists in permissions ";
            WAR_THROW_T(ExceptionAlreadyExist, "ppath");
        }
    }

    LOG_TRACE2_FN << "Adding path " << log::Esc(path->GetVirtualPath())
        << " --> " << log::Esc(path->GetPhysPath().string())
        << " to configuration";

    paths_.push_back(move(path));
}

Permissions::ptr_t WfdePermissions::Copy() const
{
    return make_shared<WfdePermissions>(*this);
}

Permissions::pathlist_t WfdePermissions::GetPaths() const
{
    pathlist_t rval;
    for(const auto& path: paths_) {
        rval.push_back(path.get());
    }

    return rval;
}

void WfdePermissions::Merge(const Permissions& perms)
{
    const auto their_paths = perms.GetPaths();

    // Add only the paths they have that I miss
    // Compare both vpaths and ppaths

    auto& vpix = paths_.get<mpaths::tag_vpath>();
    auto& ppix = paths_.get<mpaths::tag_ppath>();

    for(const auto p: their_paths) {

        if (vpix.find(p->GetVirtualPath()) != vpix.end()) {
            LOG_TRACE2_FN << "Skipping existing "
                << log::Esc(p->GetVirtualPath());
            continue;
        }

        if (ppix.find(p->GetPhysPath()) != ppix.end()) {
            LOG_DEBUG_FN << "The ppath " << log::Esc(p->GetPhysPath().string())
                << " already exists in permissions under another vpath";
            continue;
        }

        // Add the path
        paths_.push_back(move(p->Copy()));
    }
}


} // impl

std::shared_ptr<Permissions> CreatePermissions(Configuration::ptr_t conf)
{
    auto rval_perms = make_shared<impl::WfdePermissions>();

    /*
     * {"/Server/Hosts/FanClub/Path/root/Name", "/"},
     * {"/Server/Hosts/FanClub/Name/root/Path", "/tmp/wfdetest"},
     * {"/Server/Hosts/FanClub/Name/root/Perms", "CAN_READ,CAN_LIST,CAN_ENTER,IS_RECURSIVE"},
     */

    auto paths = conf->EnumNodes("");

    for(const auto name: paths) {
        string node_name = "/"s + name.name + "/Name"s;
        const auto vpath = conf->GetValue(node_name.c_str());

        node_name = "/"s + name.name + "/Path"s;
        const auto ppath = conf->GetValue(node_name.c_str());

        node_name = "/"s + name.name + "/Perms"s;
        const auto perms = conf->GetValue(node_name.c_str());

        auto new_path = make_unique<impl::WfdePath>(vpath, ppath, Path::ToPermBits(perms));

        rval_perms->AddPath(move(new_path));
    }

    return move(rval_perms);
}

}} // namespaceces

