#include "war_wfde.h"

#include <algorithm>

#include "WfdePath.h"
#include "log/WarLog.h"
#include "wfde/ftp_protocol.h"
#include "war_error_handling.h"

using namespace std;
using namespace std::string_literals;

namespace war {
namespace wfde {
namespace impl {

WfdePath::WfdePath(const Path::vpath_t& vpath,
                   const Path::ppath_t& ppath,
                   const Path::permbits_t permbits,
                   const Path::Type& type)
: ppath_{ppath}, vpath_{MakeValid(vpath)}, permbits_{permbits}, type_{type}
{

}

WfdePath::WfdePath(const WfdePath& v)
: ppath_{v.ppath_}, vpath_{v.vpath_}, permbits_{v.permbits_}, type_{v.type_}
{

}

Path::vpath_t WfdePath::MakeValid(const Path::vpath_t& vpath)
{
    static const auto root = "/"s;

    if (!vpath.empty() && (vpath[0] != '/'))
            return Path::vpath_t(root + vpath);

    return vpath;
}


bool WfdePath::Exists() const
{
    switch(type_) {
        case Type::FILE:
            return boost::filesystem::is_regular_file(ppath_);
        case Type::DIRECTORY:
            return boost::filesystem::is_directory(ppath_);
        case Type::ANY:
            return boost::filesystem::exists(ppath_);
    }

    WAR_ASSERT(false && "We cannot possibly get here");
    return false; // Make g++ happy
}

bool WfdePath::IsSameParentDir(const Path& path)
{
    auto my_parts = Split(vpath_);
    auto theirs_parts = Split(vpath_);

    if (my_parts.empty() || (my_parts.size() != theirs_parts.size()))
        return false;

    return equal(my_parts.begin(), my_parts.end() -1,
                 theirs_parts.begin(), --theirs_parts.end() -1);
}


unique_ptr<Path> WfdePath::Copy() const
{
    return make_unique<WfdePath>(*this);
}

unique_ptr< Path > WfdePath::Copy(Path::permbits_t bits) const
{
    return make_unique<WfdePath>(vpath_, ppath_, bits, type_);
}


unique_ptr< Path > WfdePath::CreateSubpath(const Path::vpath_t& subPath,
                                           const Type pathType) const
{

    WAR_ASSERT(subPath.empty() || (subPath[0] != '/'));
    WAR_ASSERT(subPath.find("../") == subPath.npos);
    WAR_ASSERT(subPath.find("//") == subPath.npos);

    LOG_TRACE4_FN << "Creating sub-path " << log::Esc(subPath)
        << " for path " << log::Esc(vpath_);

    auto parts = Split(subPath);
    ppath_t pp = ppath_;
    vpath_t vp = vpath_;

    for (const auto& p : parts) {

        const std::string node = p.to_string();

        pp /= node;

        // Special case - the root-path is '/' - all other paths ends in non-slash
        if (vp.size() > 1)
            vp += '/';

        vp += node;

        LOG_DEBUG_FN << "p=" << log::Esc(p)
            << ", pp=" << log::Esc(pp.string())
            << ", vp=" << log::Esc(vp);
    }

    return make_unique<WfdePath>(vp, pp, permbits_, pathType);
}

vector< boost::string_ref > WfdePath::DoSplit(const string& partsToSplit,
                                              const char splitCh)
{
    vector<boost::string_ref> parts;

    auto bytes_left = partsToSplit.size();
    decltype(bytes_left) pos = 0;

    while(bytes_left) {
        // Strip off leading slash(es)
        if(partsToSplit[pos] == splitCh) {
            ++pos;
            --bytes_left;
            continue;
        }

        auto seg_len = partsToSplit.find_first_of(splitCh, pos);
        if (seg_len == partsToSplit.npos) {
            // Last segment
            seg_len = bytes_left;
        } else {
            seg_len -= pos;
        }

        WAR_ASSERT(seg_len != 0);
        WAR_ASSERT(partsToSplit.at(pos + (seg_len -1)) != splitCh);

        parts.emplace_back(&partsToSplit[pos], seg_len);

        bytes_left -= seg_len;
        pos += seg_len;
    }

    return parts;
}

Path::permbits_t WfdePath::ToPermBit(const boost::string_ref& name)
{
    static const initializer_list<pair<string, PermissionBits>> perms = {
        {"CAN_READ", PermissionBits::CAN_READ},
        {"CAN_WRITE", PermissionBits::CAN_WRITE},
        {"CAN_EXECUTE", PermissionBits::CAN_EXECUTE},
        {"CAN_ENTER", PermissionBits::CAN_ENTER},
        {"CAN_LIST", PermissionBits::CAN_LIST},
        {"CAN_CREATE_DIR", PermissionBits::CAN_CREATE_DIR},
        {"CAN_CREATE_FILE", PermissionBits::CAN_CREATE_FILE},
        {"CAN_DELETE_FILE", PermissionBits::CAN_DELETE_FILE},
        {"CAN_DELETE_DIR", PermissionBits::CAN_DELETE_DIR},
        {"CAN_SEE_HIDDEN_FILES", PermissionBits::CAN_SEE_HIDDEN_FILES},
        {"CAN_SEE_HIDDEN_DIRS", PermissionBits::CAN_SEE_HIDDEN_DIRS},
        {"CAN_CREATE_HIDDEN_FILES", PermissionBits::CAN_CREATE_HIDDEN_FILES},
        {"CAN_CREATE_HIDDEN_DIRS", PermissionBits::CAN_CREATE_HIDDEN_DIRS},
        {"CAN_SET_TIMESTAMP", PermissionBits::CAN_SET_TIMESTAMP},
        {"CAN_SET_PERMISSIONS", PermissionBits::CAN_SET_PERMISSIONS},
        {"CAN_RENAME", PermissionBits::CAN_RENAME},
        {"IS_RECURSIVE", PermissionBits::IS_RECURSIVE},
        {"IS_SHARED_UPLOAD_DIR", PermissionBits::IS_SHARED_UPLOAD_DIR},
    };

    for(const auto& p : perms) {
        if (name == p.first) {
            return Bit(p.second);
        }
    }

    WAR_THROW_T(ExceptionNotFound, name.data());
}

} // impl

Path::permbits_t Path::ToPermBits(const initializer_list<PermissionBits>& list) noexcept
{
    permbits_t rval = 0;

    for(const auto p : list) {
        rval |= static_cast<permbits_t>(p);
    }

    return rval;
}

Path::permbits_t Path::GetDefaultPermissions() noexcept
{
    return impl::WfdePath::Bit(PermissionBits::CAN_LIST)
        | impl::WfdePath::Bit(PermissionBits::CAN_ENTER)
        | impl::WfdePath::Bit(PermissionBits::CAN_READ)
        | impl::WfdePath::Bit(PermissionBits::IS_RECURSIVE);
}

Path::permbits_t Path::GetDefaultHomePermissions() noexcept
{
    return impl::WfdePath::Bit(PermissionBits::CAN_LIST)
    | impl::WfdePath::Bit(PermissionBits::CAN_ENTER)
    | impl::WfdePath::Bit(PermissionBits::CAN_READ)
    | impl::WfdePath::Bit(PermissionBits::CAN_WRITE)
    | impl::WfdePath::Bit(PermissionBits::CAN_CREATE_FILE)
    | impl::WfdePath::Bit(PermissionBits::CAN_DELETE_FILE)
    | impl::WfdePath::Bit(PermissionBits::CAN_RENAME)
    | impl::WfdePath::Bit(PermissionBits::CAN_CREATE_DIR)
    | impl::WfdePath::Bit(PermissionBits::CAN_DELETE_DIR)
    | impl::WfdePath::Bit(PermissionBits::CAN_SET_TIMESTAMP)
    | impl::WfdePath::Bit(PermissionBits::IS_RECURSIVE)
    | impl::WfdePath::Bit(PermissionBits::CAN_SET_PERMISSIONS);
}

Path::permbits_t Path::GetDefaultPubUploadPermissions() noexcept
{
    return impl::WfdePath::Bit(PermissionBits::CAN_LIST)
    | impl::WfdePath::Bit(PermissionBits::CAN_ENTER)
    | impl::WfdePath::Bit(PermissionBits::IS_SHARED_UPLOAD_DIR);
}

Path::permbits_t Path::ToPermBits(const std::string& list)
{
    Path::permbits_t rval = 0;

    auto perms = impl::WfdePath::DoSplit(list, ',');

    for(const auto& perm : perms) {
        rval |= impl::WfdePath::ToPermBit(perm);
    }

    return rval;
}

vector<boost::string_ref> Path::Split(const Path::vpath_t& partsToSplit)
{
    return impl::WfdePath::DoSplit(partsToSplit);
}


/* From a security perspective this method is one of the most important
 * pices of code in the entire library.
 *
 * Please read the code carefully and report any potential glitches
 * immediately.
 */
vector<boost::string_ref> Path::NormalizeAndSplit(const Path::vpath_t& vpath,
                                                  const vpath_t& currentDir)
{
    if (vpath.empty()) {
        return Split(currentDir);
    }

    vector<boost::string_ref> parts;

    if (vpath.front() != '/') {
        // Relative path, which means we are relative to CWD.
        parts = Split(currentDir);
    }

    // Parse the vpath.
    const auto end = vpath.cend();
    auto cur = vpath.cbegin();

    while(cur != end) {
        if (*cur == '/') {
            // - Skip it so we can get started with the next part
            ++cur;
            continue;
        }

        if (*cur == '.') {
            auto p = cur;

            if (++p == end) {
                // Path ends with a single . - Strip it off
                cur = p;
                continue;
            }

            if (*p == '/') {
                // ./ - Just strip it off
                cur = ++p;
                continue;
            }

            if (*p == '.') {
                // .. - Investigate further
                auto pp = p;

                if ((++pp == end) || (*pp == '/')) {
                    /// Current segment starts with ..[/] - Wind up one part
                    if (parts.empty()) {
                        // We are already at the top - Not valid!
                        WAR_THROW_T(ExceptionAccessDenied, vpath);
                    }

                    parts.resize(parts.size() -1);
                    cur = pp;
                    if (pp != end)
                        ++cur;
                    continue;
                }

                // We don't allow dot-dot-anything but "../"
                WAR_THROW_T(ExceptionBadPath, vpath);
            } // ..
        } // .

        // Not  / or ./ or ../

        auto p = cur;
        for(; (p != end) && (*p != '/'); ++p) {
            if (UNLIKELY(*p == '\\')) {
                // We don't allow back-slashes
                WAR_THROW_T(ExceptionBadPath, vpath);
            }

            if ((*p == '.') && (p != cur)
                && ((*(p+1) == '.' || *(p+1) == '/'))) {
                break;
            }
        }

        if (p != cur) {
            parts.emplace_back(&(*cur), p - cur);
            cur = p;
            continue;
        }

        cout << "cur=" << *cur << endl;
        WAR_ASSERT(cur == end);
    }

    return parts;
}

Path::vpath_t Path::Normalize(const Path::vpath_t& vpath,
                              const Path::vpath_t& currentDir)
{
    return ToVpath(NormalizeAndSplit(vpath, currentDir), true);
}

Path::vpath_t Path::ToVpath(const std::vector<boost::string_ref>& parts,
                            const bool addRoot)
{
    Path::vpath_t vpath;

    // Reserve the required space in vpath so we don't do multiple allocations
    {
        decltype(vpath.size()) len = 1;
        for(const auto& part: parts) {
            len += part.size() + 1;
        }

        vpath.reserve(len);
    }

    if (addRoot)
        vpath = '/';

    bool virgin = true;
    for(const auto& part: parts) {
        if (!virgin)
            vpath += '/';
        else
            virgin = false;

        vpath.append(part.data(), part.size());
    }

    return vpath;
}

boost::string_ref impl::WfdePath::GetVpathFileName() const
{
    auto pos = vpath_.find_last_of('/');
    if (pos == vpath_.npos)
        return boost::string_ref{};

    return boost::string_ref{vpath_.c_str() + pos, vpath_.size() - pos};
}


}} // wfde war
