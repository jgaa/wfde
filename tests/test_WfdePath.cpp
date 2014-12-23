#include "war_tests.h"
#include "wfde/wfde.h"
#include "../src/wfde/WfdePath.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace war;
using namespace war::wfde;
using namespace war::wfde::impl;

#define PB(b) WfdePath::Bit(Path::PermissionBits::b), Path::Type::DIRECTORY

BOOST_AUTO_TEST_SUITE(Wfde_Unit_Tests)

BOOST_AUTO_TEST_CASE(Test_WfdePath)
{
    BOOST_CHECK(WfdePath("", "", PB(CAN_READ)).CanRead());
    BOOST_CHECK(!WfdePath("", "", PB(CAN_READ)).CanWrite());

    BOOST_CHECK(WfdePath("", "", PB(CAN_WRITE)).CanWrite());
    BOOST_CHECK(WfdePath("", "", PB(CAN_EXECUTE)).CanExecute());
    BOOST_CHECK(WfdePath("", "", PB(CAN_ENTER)).CanEnter());
    BOOST_CHECK(WfdePath("", "", PB(CAN_LIST)).CanList());
    BOOST_CHECK(WfdePath("", "", PB(CAN_CREATE_DIR)).CanCreateDir());
    BOOST_CHECK(WfdePath("", "", PB(CAN_CREATE_FILE)).CanCreateFile());
    BOOST_CHECK(WfdePath("", "", PB(CAN_DELETE_FILE)).CanDeleteFile());
    BOOST_CHECK(WfdePath("", "", PB(CAN_DELETE_DIR)).CanDeleteDir());
    BOOST_CHECK(WfdePath("", "", PB(CAN_SEE_HIDDEN_FILES)).CanSeeHiddenFiles());
    BOOST_CHECK(WfdePath("", "", PB(CAN_SEE_HIDDEN_DIRS)).CanSeeHiddenDirs());
    BOOST_CHECK(WfdePath("", "", PB(CAN_CREATE_HIDDEN_FILES)).CanCreateHiddenFile());
    BOOST_CHECK(WfdePath("", "", PB(CAN_CREATE_HIDDEN_DIRS)).CanCreateHiddenDir());
    BOOST_CHECK(WfdePath("", "", PB(CAN_SET_TIMESTAMP)).CanSetTimestamp());
    BOOST_CHECK(WfdePath("", "", PB(CAN_SET_PERMISSIONS)).CanSetPermissions());
    BOOST_CHECK(WfdePath("", "", PB(CAN_RENAME)).CanRename());
    BOOST_CHECK(WfdePath("", "", PB(IS_RECURSIVE)).IsRecursive());
    BOOST_CHECK(WfdePath("", "", PB(IS_SHARED_UPLOAD_DIR)).IsSharedUploadDir());

    BOOST_CHECK(WfdePath("", "", 0, Path::Type::DIRECTORY).IsDirectory());
    BOOST_CHECK(!WfdePath("", "", 0, Path::Type::DIRECTORY).IsFile());
    BOOST_CHECK(!WfdePath("", "", 0, Path::Type::FILE).IsDirectory());
    BOOST_CHECK(WfdePath("", "", 0, Path::Type::FILE).IsFile());
    BOOST_CHECK(!WfdePath("", "", 0, Path::Type::ANY).IsDirectory());
    BOOST_CHECK(!WfdePath("", "", 0, Path::Type::ANY).IsFile());

    // Normalize
    BOOST_CHECK(Path::Normalize("jgaa", "/home") == "/home/jgaa");
    BOOST_CHECK(Path::Normalize("/home/jgaa", "/home") == "/home/jgaa");
    BOOST_CHECK(Path::Normalize("/home/jgaa/docs", "/home") == "/home/jgaa/docs");
    BOOST_CHECK(Path::Normalize("/pub", "/home") == "/pub");
    BOOST_CHECK(Path::Normalize("../pub", "/home") == "/pub");
    BOOST_CHECK(Path::Normalize("./../pub", "/home") == "/pub");
    BOOST_CHECK(Path::Normalize("./../pub/src/whid/../../", "/homess") == "/pub");
    BOOST_CHECK(Path::Normalize("./", "/home") == "/home");
    BOOST_CHECK(Path::Normalize("../", "/homes") == "/");
    BOOST_CHECK(Path::Normalize("/test/there/.", "/homes") == "/test/there");
    BOOST_CHECK(Path::Normalize(".", "/home/jgaa/docs/c++books/standards/1/99")
        == "/home/jgaa/docs/c++books/standards/1/99");
    BOOST_CHECK(Path::Normalize("../.", "/home/jgaa/docs/c++books/standards/1/99")
        == "/home/jgaa/docs/c++books/standards/1");
    BOOST_CHECK(Path::Normalize("/pub/..", "/home") == "/");
    BOOST_CHECK(Path::Normalize("readme.txt", "/") == "/readme.txt");

    // Try some invalid paths
    BOOST_REQUIRE_THROW(Path::Normalize("../../", "/home"),
                        war::ExceptionAccessDenied);
    BOOST_REQUIRE_THROW(Path::Normalize("/../", "/home"),
                        war::ExceptionAccessDenied);
    BOOST_REQUIRE_THROW(Path::Normalize("/..", "/home"),
                        war::ExceptionAccessDenied);
    BOOST_REQUIRE_THROW(Path::Normalize("./../pub/src/whid/../../../../", "/home"),
                        war::ExceptionAccessDenied);
    BOOST_REQUIRE_THROW(Path::Normalize("/../", "/home/test"),
                        war::ExceptionAccessDenied);
    BOOST_REQUIRE_THROW(Path::Normalize("test/...", "/home/test"),
                        war::ExceptionBadPath);
    BOOST_REQUIRE_THROW(Path::Normalize("/test/.../there", "/home/test"),
                        war::ExceptionBadPath);
}

BOOST_AUTO_TEST_SUITE_END()
