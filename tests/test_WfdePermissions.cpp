
#include "war_tests.h"
#include <wfde/wfde.h>
#include "../src/wfde/WfdePermissions.h"
#include "../src/wfde/WfdePath.h"

using namespace std;
using namespace war;
using namespace war::wfde;
using namespace war::wfde::impl;

const lest::test specification[] = {

STARTCASE(Test_PathAccessFromPermissions)
{
    WfdePermissions perms;

    // Add some paths
    EXPECT_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/", "/tmp/ftptest",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            Path::PermissionBits::CAN_READ,
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    );

    EXPECT_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/home", "/tmp/homedirs",
                                        Path::ToPermBits({
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    );

     EXPECT_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/home/princess", "/tmp/princess",
                                        Path::ToPermBits({
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    );

    EXPECT_NO_THROW(
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

    EXPECT_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/pub", "/tmp/pubfiles",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            Path::PermissionBits::CAN_READ,
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    );

    EXPECT_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/pub/src", "/tmp/pub/src",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            }), Path::Type::DIRECTORY));
    );

    EXPECT_NO_THROW(
    perms.AddPath(make_unique<WfdePath>("/pub/src/warftpd", "/tmp/warscr",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            Path::PermissionBits::CAN_READ,
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    );

    // Verify that we can't add duplicate vpaths
    EXPECT_THROWS_AS(
    perms.AddPath(make_unique<WfdePath>("/home", "/tmp/ftptest_",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            Path::PermissionBits::CAN_READ,
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    , war::ExceptionAlreadyExist);

    // Verify that we can't add duplicate ppaths
    EXPECT_THROWS_AS(
    perms.AddPath(make_unique<WfdePath>("/aa", "/tmp/ftptest",
                                        Path::ToPermBits({
                                            Path::PermissionBits::CAN_ENTER,
                                            Path::PermissionBits::CAN_LIST,
                                            Path::PermissionBits::CAN_READ,
                                            Path::PermissionBits::IS_RECURSIVE
                                            }), Path::Type::DIRECTORY));
    , war::ExceptionAlreadyExist);



    // Verify that we get the correct ones back
    EXPECT(perms.GetPath("/").GetVirtualPath() == "/");
    EXPECT(perms.GetPath("/pub").GetPhysPath().string() == "/tmp/pubfiles");
    EXPECT(perms.GetPath("/pub/src").GetPhysPath().string() == "/tmp/pub/src");
    EXPECT(perms.GetPath("/pub/src/warftpd/foo/foofoo").GetVirtualPath()
        == "/pub/src/warftpd");

    // Check that we can resolve undefined subpaths
    Path::vpath_t remaining;
    EXPECT(perms.GetPath("/nonexistant", &remaining).GetPhysPath().string()
        == "/tmp/ftptest");
    EXPECT(remaining == "nonexistant");

    EXPECT(perms.GetPath("/a/b/c/dd/e", &remaining).GetPhysPath().string()
        == "/tmp/ftptest");
    EXPECT(remaining == "a/b/c/dd/e");

    // Check recursive permissions
    EXPECT(perms.GetPath("/home/root").CanEnter() == false);
    EXPECT(perms.GetPath("/home").CanEnter() == false);
    EXPECT(perms.GetPath("/home/jgaa").CanEnter() == true);
    EXPECT(perms.GetPath("/pub/src").CanList() == true);
    EXPECT(perms.GetPath("/pub/src").CanRead() == false);
    EXPECT(perms.GetPath("/pub/src/warftpd").CanRead() == true);
    EXPECT(perms.GetPath("/pub/src/warftpd/test/tests/g.cpp").CanRead() == true);

    // Check that we can't access subpaths of non-recursive paths
    EXPECT_THROWS_AS(perms.GetPath("/pub/src/whid"), war::ExceptionAccessDenied);
    EXPECT_THROWS_AS(perms.GetPath("/pub/src/whid/secrets"), war::ExceptionAccessDenied);

} ENDCASE
}; //lest

int main( int argc, char * argv[] )
{
    log::LogEngine log;
    log.AddHandler(log::LogToFile::Create("Test_ServerObject.log", true, "file",
        log::LL_TRACE4, log::LA_DEFAULT_ENABLE | log::LA_THREADS) );

    return lest::run( specification, argc, argv );
}

