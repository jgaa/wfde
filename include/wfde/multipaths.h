#include "wfde.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/sequenced_index.hpp>

namespace war {
namespace wfde {
namespace mpaths {


using path_ptr_t =  std::unique_ptr<Path>;

/// Virtual path tag for multi-index
struct tag_vpath
{
    using result_type = Path::vpath_t;
    const result_type& operator()(const path_ptr_t& p) const { return p->GetVirtualPath(); }
};

/// File system path tag for multi-index
struct tag_ppath
{
    using result_type = Path::ppath_t;
    const result_type& operator()(const path_ptr_t& p) const { return p->GetPhysPath(); }
};

/*! Path container.
    Indexed on
        - sequenced for easy iteration with for()
        - tag_vpath Virtual path, as seen by the client
        - tag_ppath Physical path on the file system
    Both indexes are unique.
*/

using multi_paths_t = boost::multi_index::multi_index_container<
    path_ptr_t,
    boost::multi_index::indexed_by<
        boost::multi_index::sequenced<>,
        boost::multi_index::ordered_unique<boost::multi_index::tag<tag_vpath>, tag_vpath>,
        boost::multi_index::ordered_unique<boost::multi_index::tag<tag_ppath>, tag_ppath>
    >
>;

using paths_t = multi_paths_t;

/*! Access paths_t from index tag_vpath */
using paths_by_vpath_t = multi_paths_t::index<tag_vpath>::type;

/*! Access paths_t from index tag_ppath */
using paths_by_fspath_t = multi_paths_t::index<tag_ppath>::type;


}}} // namespaces

