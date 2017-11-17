#include "war_tests.h"
#include <wfde/wfde.h>
#include "../src/wfde/WfdePath.h"

using namespace std;
using namespace war;
using namespace war::wfde;
using namespace war::wfde::impl;

#define PB(b) WfdePath::Bit(Path::PermissionBits::b), Path::Type::DIRECTORY

const lest::test specification[] = {

STARTCASE(Test_WfdePath)
{
    EXPECT(WfdePath("", "", PB(CAN_READ)).CanRead());
    EXPECT(!WfdePath("", "", PB(CAN_READ)).CanWrite());

    EXPECT(WfdePath("", "", PB(CAN_WRITE)).CanWrite());
    EXPECT(WfdePath("", "", PB(CAN_EXECUTE)).CanExecute());
    EXPECT(WfdePath("", "", PB(CAN_ENTER)).CanEnter());
    EXPECT(WfdePath("", "", PB(CAN_LIST)).CanList());
    EXPECT(WfdePath("", "", PB(CAN_CREATE_DIR)).CanCreateDir());
    EXPECT(WfdePath("", "", PB(CAN_CREATE_FILE)).CanCreateFile());
    EXPECT(WfdePath("", "", PB(CAN_DELETE_FILE)).CanDeleteFile());
    EXPECT(WfdePath("", "", PB(CAN_DELETE_DIR)).CanDeleteDir());
    EXPECT(WfdePath("", "", PB(CAN_SEE_HIDDEN_FILES)).CanSeeHiddenFiles());
    EXPECT(WfdePath("", "", PB(CAN_SEE_HIDDEN_DIRS)).CanSeeHiddenDirs());
    EXPECT(WfdePath("", "", PB(CAN_CREATE_HIDDEN_FILES)).CanCreateHiddenFile());
    EXPECT(WfdePath("", "", PB(CAN_CREATE_HIDDEN_DIRS)).CanCreateHiddenDir());
    EXPECT(WfdePath("", "", PB(CAN_SET_TIMESTAMP)).CanSetTimestamp());
    EXPECT(WfdePath("", "", PB(CAN_SET_PERMISSIONS)).CanSetPermissions());
    EXPECT(WfdePath("", "", PB(CAN_RENAME)).CanRename());
    EXPECT(WfdePath("", "", PB(IS_RECURSIVE)).IsRecursive());
    EXPECT(WfdePath("", "", PB(IS_SHARED_UPLOAD_DIR)).IsSharedUploadDir());

    EXPECT(WfdePath("", "", 0, Path::Type::DIRECTORY).IsDirectory());
    EXPECT(!WfdePath("", "", 0, Path::Type::DIRECTORY).IsFile());
    EXPECT(!WfdePath("", "", 0, Path::Type::FILE).IsDirectory());
    EXPECT(WfdePath("", "", 0, Path::Type::FILE).IsFile());
    EXPECT(!WfdePath("", "", 0, Path::Type::ANY).IsDirectory());
    EXPECT(!WfdePath("", "", 0, Path::Type::ANY).IsFile());

    // Normalize
    EXPECT(Path::Normalize("jgaa", "/home") == "/home/jgaa");
    EXPECT(Path::Normalize("/home/jgaa", "/home") == "/home/jgaa");
    EXPECT(Path::Normalize("/home/jgaa/docs", "/home") == "/home/jgaa/docs");
    EXPECT(Path::Normalize("/pub", "/home") == "/pub");
    EXPECT(Path::Normalize("../pub", "/home") == "/pub");
    EXPECT(Path::Normalize("./../pub", "/home") == "/pub");
    EXPECT(Path::Normalize("./../pub/src/whid/../../", "/homess") == "/pub");
    EXPECT(Path::Normalize("./", "/home") == "/home");
    EXPECT(Path::Normalize("../", "/homes") == "/");
    EXPECT(Path::Normalize("/test/there/.", "/homes") == "/test/there");
    EXPECT(Path::Normalize(".", "/home/jgaa/docs/c++books/standards/1/99")
        == "/home/jgaa/docs/c++books/standards/1/99");
    EXPECT(Path::Normalize("../.", "/home/jgaa/docs/c++books/standards/1/99")
        == "/home/jgaa/docs/c++books/standards/1");
    EXPECT(Path::Normalize("/pub/..", "/home") == "/");
    EXPECT(Path::Normalize("readme.txt", "/") == "/readme.txt");

    // Try some invalid paths
    EXPECT_THROWS_AS(Path::Normalize("../../", "/home"),
                        war::ExceptionAccessDenied);
    EXPECT_THROWS_AS(Path::Normalize("/../", "/home"),
                        war::ExceptionAccessDenied);
    EXPECT_THROWS_AS(Path::Normalize("/..", "/home"),
                        war::ExceptionAccessDenied);
    EXPECT_THROWS_AS(Path::Normalize("./../pub/src/whid/../../../../", "/home"),
                        war::ExceptionAccessDenied);
    EXPECT_THROWS_AS(Path::Normalize("/../", "/home/test"),
                        war::ExceptionAccessDenied);
    EXPECT_THROWS_AS(Path::Normalize("test/...", "/home/test"),
                        war::ExceptionBadPath);
    EXPECT_THROWS_AS(Path::Normalize("/test/.../there", "/home/test"),
                        war::ExceptionBadPath);
} ENDCASE
}; //lest

int main( int argc, char * argv[] )
{
    log::LogEngine log;
    log.AddHandler(log::LogToFile::Create("Test_ServerObject.log", true, "file",
        log::LL_TRACE4, log::LA_DEFAULT_ENABLE | log::LA_THREADS) );

    return lest::run( specification, argc, argv );
}
