
#include "war_tests.h"
#include <wfde/wfde.h>
#include "../src/wfde/WfdePermissions.h"
#include "../src/wfde/WfdePath.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace war;
using namespace war::wfde;
using namespace war::wfde::impl;


BOOST_AUTO_TEST_SUITE(Wfde_Unit_Tests)

BOOST_AUTO_TEST_CASE(Test_PathAccessFromPermissions)
{
    WfdePermissions perms;

    // Add some paths
    BOOST_REQUIRE_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/", "/tmp/ftptest",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            Path::PermissionBits::CAN_READ,
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    );

    BOOST_REQUIRE_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/home", "/tmp/homedirs",
                                        Path::ToPermBits({
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    );

     BOOST_REQUIRE_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/home/princess", "/tmp/princess",
                                        Path::ToPermBits({
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    );

    BOOST_REQUIRE_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/home/jgaa", "/tmp/homedirs/jgaa",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            Path::PermissionBits::CAN_READ,
                                            Path::PermissionBits::CAN_WRITE,
                                            Path::PermissionBits::CAN_CREATE_FILE,
                                            Path::PermissionBits::CAN_SET_PERMISSIONS,
                                            Path::PermissionBits::CAN_SET_TIMESTAMP,
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    );

    BOOST_REQUIRE_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/pub", "/tmp/pubfiles",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            Path::PermissionBits::CAN_READ,
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    );

    BOOST_REQUIRE_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/pub/src", "/tmp/pub/src",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            }), Path::Type::DIRECTORY));
    );

    BOOST_REQUIRE_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/pub/src/warftpd", "/tmp/warscr",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            Path::PermissionBits::CAN_READ,
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    );

    // Verify that we can't add duplicate vpaths
    BOOST_REQUIRE_THROW(
    perms.AddPath(make_unique<WfdePath>("/home", "/tmp/ftptest_",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            Path::PermissionBits::CAN_READ,
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    , war::ExceptionAlreadyExist);

    // Verify that we can't add duplicate ppaths
    BOOST_REQUIRE_THROW(
    perms.AddPath(make_unique<WfdePath>("/aa", "/tmp/ftptest",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            Path::PermissionBits::CAN_READ,
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    , war::ExceptionAlreadyExist);



    // Verify that we get the correct ones back
    BOOST_CHECK(perms.GetPath("/").GetVirtualPath() == "/");
    BOOST_CHECK(perms.GetPath("/pub").GetPhysPath() == "/tmp/pubfiles");
    BOOST_CHECK(perms.GetPath("/pub/src").GetPhysPath() == "/tmp/pub/src");
    BOOST_CHECK(perms.GetPath("/pub/src/warftpd/foo/foofoo").GetVirtualPath()
        == "/pub/src/warftpd");

    // Check that we can resolve undefined subpaths
    Path::vpath_t remaining;
    BOOST_CHECK(perms.GetPath("/nonexistant", &remaining).GetPhysPath()
        == "/tmp/ftptest" && remaining == "nonexistant");

    BOOST_CHECK(perms.GetPath("/a/b/c/dd/e", &remaining).GetPhysPath()
        == "/tmp/ftptest" && remaining == "a/b/c/dd/e");

    // Check recursive permissions
    BOOST_CHECK(perms.GetPath("/home/root").CanEnter() == false);
    BOOST_CHECK(perms.GetPath("/home").CanEnter() == false);
    BOOST_CHECK(perms.GetPath("/home/jgaa").CanEnter() == true);
    BOOST_CHECK(perms.GetPath("/pub/src").CanList() == true);
    BOOST_CHECK(perms.GetPath("/pub/src").CanRead() == false);
    BOOST_CHECK(perms.GetPath("/pub/src/warftpd").CanRead() == true);
    BOOST_CHECK(perms.GetPath("/pub/src/warftpd/test/tests/g.cpp").CanRead() == true);

    // Check that we can't access subpaths of non-recursive paths
    BOOST_REQUIRE_THROW(perms.GetPath("/pub/src/whid"), war::ExceptionAccessDenied);
    BOOST_REQUIRE_THROW(perms.GetPath("/pub/src/whid/secrets"), war::ExceptionAccessDenied);

}

BOOST_AUTO_TEST_SUITE_END()
